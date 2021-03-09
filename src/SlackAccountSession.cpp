#include "SlackAccountSession.h"
#include "include/ITeam.h"
#include "PolySlack.h"
#include "include/Message.h"
#include <chrono>
#include <memory>

using namespace PolySlackPlugin;

SlackAccountSession::SlackAccountSession(Polychat::IAccount& coreAccount, std::string token,
	std::string uid, Polychat::ICore& core)
:
	coreAccount(coreAccount), core(core), token(token), uid(uid),
	webClient(core.getWebHelper().initHTTPClient("slack.com", 443, true))
{
	loadEventFunctions();
	tokenIsValid = true;

	getFullUserDetails();
	initWebsockets();
}

void SlackAccountSession::initWebsockets() {
	std::shared_ptr<HTTPStringContent> content = std::make_shared<HTTPStringContent>("");
	content->setContentType("application/x-www-form-urlencoded");

	std::string url = "/api/rtm.connect/"; // replace with rtm.start for more useful info
	HTTPMessage message(HTTPMethod::GET, url);
	message.setContent(content);
	message.setAuthorization("Bearer", token);

	IWebHelper& web = core.getWebHelper();

	webClient->sendRequest(message, [this](std::shared_ptr<HTTPMessage> responsePtr) {
		if (responsePtr->getStatus() == HTTPStatus::HTTP_OK) {
			HTTPMessage& response = *responsePtr.get();
			auto userObj = nlohmann::json::parse(response.getContent()->getAsString());
			bool isOkay = userObj["ok"];
			if (isOkay == true) {
				std::string url = userObj["url"];
				std::string host, uri;
				unsigned int port;
				bool ssl;
				core.getWebHelper().parseAddress(url, host, port, ssl, uri);
				webSocketConnection = core.getWebHelper().initWebsocket(host, port, ssl, uri);
				webSocketConnection->setOnStringReceived(std::bind(&SlackAccountSession::onWSMessageReceived, this, std::placeholders::_1));
				webSocketConnection->setOnWebsocketOpen(std::bind(&SlackAccountSession::onWSOpen, this));
				webSocketConnection->open();
			} else {
				core.alert(userObj["error"]);
			}
		} else {
			core.alert(std::to_string((int)responsePtr->getStatus()));
		}
	});

	

}

void SlackAccountSession::getFullUserDetails() {
	std::shared_ptr<HTTPStringContent> content = std::make_shared<HTTPStringContent>("");
	content->setContentType("application/x-www-form-urlencoded");

	std::string url = "/api/users.info/"; // or maybe users.profile.get
	url += "?user=" + uid;
	HTTPMessage message(HTTPMethod::GET, url);
	message.setContent(content);
	message.setAuthorization("Bearer", token);
	
	IWebHelper& web = core.getWebHelper();

	webClient->sendRequest(message, [this](std::shared_ptr<HTTPMessage> responsePtr) {
		if (responsePtr->getStatus() == HTTPStatus::HTTP_OK) {
			HTTPMessage& response = *responsePtr.get();
			auto userObj = nlohmann::json::parse(response.getContent()->getAsString());
			bool isOkay = userObj["ok"];
			if (isOkay == true) {
				auto user = userObj["user"];
				auto profile = user["profile"];
				teamID = userObj["team_id"];
				// already got name
				std::string first_name = profile["first_name"];
				std::string last_name = profile["last_name"];
				std::string email = profile["email"];
				std::string displayName = profile["display_name"];
				// There are also a lot of other fields that will
				// eventually be useful.
				coreAccount.setNickName(displayName);
					
				updateTeams(true);
					
			} else {
				core.alert(userObj["error"]);
			}
		} else {
			core.alert(std::to_string((int)responsePtr->getStatus()));
		}
	});
}

void SlackAccountSession::onWSMessageReceived(std::string msg) {
	nlohmann::json json = nlohmann::json::parse(msg);
	std::cout << "Websocket received:" << msg << std::endl;

	nlohmann::json eventJSON = nlohmann::json::parse(msg);
	std::string eventName = eventJSON["type"];

	std::function<void(SlackAccountSession*, nlohmann::json)> funcForEvent = eventMap[eventName];

	if (funcForEvent) {
		funcForEvent(this, eventJSON);
	} else {
		core.alert("Unknown event \"" + eventName + "\"");
	}
}

void SlackAccountSession::onWSOpen() {
	// Nothing needed on open. It's already authenticated.
}


void SlackAccountSession::loadEventFunctions() {
	eventMap["message"] = &SlackAccountSession::onMessageEvent;
}

// Epoch in milliseconds
unsigned long long timeToLong(std::string& ts) {
	// Using double precision shouldn't result in a loss of resolution
	return static_cast<unsigned long long>(stod(ts) * 1000000);
}

std::shared_ptr<Polychat::Message> getMsgFromJSON(nlohmann::json postJSON) {
	std::shared_ptr<Polychat::Message> newMSG = std::make_shared<Polychat::Message>();
	newMSG->id = postJSON.value("ts", "unknown ID");
	newMSG->uid = postJSON.value("user", "unknown user");
	newMSG->channelId = postJSON.value("channel", "unknown channel");
	// TODO: Check that the timestamp units are correct.
	newMSG->createdAt = timeToLong(postJSON.value("ts","0"));
	if (postJSON.contains("edited")) {
		auto editedSection = postJSON["edited"];
		newMSG->editedAt = timeToLong(editedSection.value("ts", "0"));
	} else {
		newMSG->editedAt = 0;
	}
	newMSG->updatedAt = 0;
	newMSG->deletedAt = 0;
	newMSG->msgContent = postJSON.value("text", "");

	return newMSG;
}

void SlackAccountSession::onMessageEvent(nlohmann::json json) {
	std::string text = json["text"];
	core.alert("On post event function called with text \"" + text + "\"");
	auto teams = coreAccount.getTeams();
	auto team = teams.find(teamID);
	if (team == teams.end()) {
		core.alert("Could not find team on post event with team id " + teamID);
		return;
	}
	auto conversationList = team->second->getConversations();
	std::string conversationID = json.at("channel").get<std::string>();
	auto conversation = conversationList.find(conversationID);
	if (conversation != conversationList.end()) {
		conversation->second->processMessage(getMsgFromJSON(json));
	} else {
		core.alert("Could not find conversation on post event");
	}
}

void SlackAccountSession::onWSClose() {
	// TODO: Try reopening?
}

void SlackAccountSession::refresh(std::shared_ptr<IConversation> currentlyViewedConversation) {
	updateTeams(true);
}

void SlackAccountSession::sendMessageAction(std::shared_ptr<Message> msg, MessageAction action) {
	if (action == MessageAction::SEND_NEW_MESSAGE) {
		msg->sendStatus = SendStatus::SENDING;

		nlohmann::json json;
		json["channel"] = msg->channelId;
		json["as_user"] = true;
		json["text"] = msg->msgContent;

		std::shared_ptr<HTTPStringContent> content = std::make_shared<HTTPStringContent>(json.dump());
		content->setContentType("application/json");

		std::string url = "/api/chat.postMessage";
		HTTPMessage message(HTTPMethod::POST, url);
		message.setContent(content);
		message.setAuthorization("Bearer", token);

		IWebHelper& web = core.getWebHelper();

		webClient->sendRequest(message, [this, msg](std::shared_ptr<HTTPMessage> responsePtr) {
			if (responsePtr->getStatus() == HTTPStatus::HTTP_OK) {
				HTTPMessage& response = *responsePtr.get();
				auto userObj = nlohmann::json::parse(response.getContent()->getAsString());
				bool isOkay = userObj["ok"];
				if (isOkay == true) {
					msg->sendStatus = SendStatus::SENT;
				} else {
					msg->sendStatus = SendStatus::FAILED;
					core.alert(userObj["error"]);
				}
			} else {
				msg->sendStatus = SendStatus::FAILED;
				core.alert(std::to_string((int)responsePtr->getStatus()));
			}
			});
	} else {
		core.alert("Send action not implemented");
	}
}


void SlackAccountSession::updatePosts(IConversation& conversation, int limit) {
	// TODO
}

bool SlackAccountSession::isValid() {
	return tokenIsValid;
}

void SlackAccountSession::updateTeams(bool updateConvs) {
	// As far as I am aware, it will be one team per user.
	std::shared_ptr<HTTPStringContent> content = std::make_shared<HTTPStringContent>("");
	content->setContentType("application/x-www-form-urlencoded");
	std::string url = "/api/team.info/";
	HTTPMessage message(HTTPMethod::GET, url);
	message.setContent(content);
	message.setAuthorization("Bearer", token);

	IWebHelper& web = core.getWebHelper();

	webClient->sendRequest(message, [this](std::shared_ptr<HTTPMessage> responsePtr) {
		if (responsePtr->getStatus() == HTTPStatus::HTTP_OK) {
			HTTPMessage& response = *responsePtr.get();
			auto resultObj = nlohmann::json::parse(response.getContent()->getAsString());
			bool isOkay = resultObj["ok"];
			if (isOkay == true) {
				auto teamObj = resultObj["team"];
				std::string teamName = teamObj["name"];

				auto accountTeams = coreAccount.getTeams();
				auto findResult = accountTeams.find(teamID);
				std::shared_ptr<ITeam> team;

				if (findResult == accountTeams.end()) {
					team = coreAccount.loadTeam(teamID, teamName, teamName);
				} else {
					team = findResult->second;
					team->setName(teamName);
					team->setDisplayName(teamName);
				}
				updateConversations(team);
			} else {
				core.alert(resultObj["error"]);
			}
		} else {
			core.alert(std::to_string((int)responsePtr->getStatus()));
		}
	});
}

void SlackAccountSession::updateConversations(std::shared_ptr<ITeam> team) {
	std::shared_ptr<HTTPStringContent> content = std::make_shared<HTTPStringContent>("");
	content->setContentType("application/x-www-form-urlencoded");
	std::string url = "/api/conversations.list/?limit=900&exclude_archived=true&types=public_channel,private_channel,mpim,im&team_id=" + teamID; // TODO: Add pagination support
	HTTPMessage message(HTTPMethod::GET, url);
	message.setContent(content);
	message.setAuthorization("Bearer", token);

	IWebHelper& web = core.getWebHelper();

	webClient->sendRequest(message, [this, team](std::shared_ptr<HTTPMessage> responsePtr) {
		if (responsePtr->getStatus() == HTTPStatus::HTTP_OK) {
			HTTPMessage& response = *responsePtr.get();
			auto resultObj = nlohmann::json::parse(response.getContent()->getAsString());
			bool isOkay = resultObj["ok"];
			if (isOkay == true) {
				auto channelsObj = resultObj["channels"];

				auto existingTeamChannels = team->getConversations();
				auto existingUserChannels = coreAccount.getConversations();

				// Set is to store which teams have been processed from the server's
				// JSON, so we know which ones don't exist, allowing us to archive them.
				std::set<std::string> serverChannelIDs;

				// Goes through each team to ensure that the core has it stored.
				// For each team, goes through all conversations.
				// TODO: Look into conversations.list and conversations.genericInfo
				for (auto& element : channelsObj) {
					std::string id = element["id"];
					std::string name = element.value("name", "No name");
					CONVERSATION_TYPE parsedType = getTypeFromJSON(element);

					serverChannelIDs.insert(id);

					// Add channel if it does not exist.
					bool newChannel;
					std::map<std::string, std::shared_ptr<Polychat::IConversation>>::iterator existingChannelsItr;
					
					existingChannelsItr = existingTeamChannels.find(id);
					newChannel = existingChannelsItr == existingTeamChannels.end();
					if (newChannel) {
						// Adds it
						std::shared_ptr<IConversation> newConversation;
						newConversation = team->addConversation(id, parsedType, name);
						
						//newConversation->setDescription(description);
						newConversation->setName(name);
					} else {
						// Gets it to ensure it's up to date.
						std::shared_ptr<IConversation> existingConversation = existingChannelsItr->second;
						if (existingConversation->getName().compare(name) != 0)
							existingConversation->setName(name);
						if (existingConversation->getTitle().compare(name) != 0)
							existingConversation->setTitle(name);
						//if (existingConversation->getDescription().compare(description) != 0)
						//	existingConversation->setDescription(description);
						if (existingConversation->getType() != parsedType)
							existingConversation->setType(parsedType);
					}
				}

			} else {
				core.alert(resultObj["error"]);
			}
		} else {
			core.alert(std::to_string((int)responsePtr->getStatus()));
		}
	});
}

CONVERSATION_TYPE SlackAccountSession::getTypeFromJSON(nlohmann::json& json) {
	if (json.value("is_channel", false)) {
		if (json.value("is_private", true)) {
			return CONVERSATION_TYPE::PRIVATE_CHANNEL;
		} else {
			return CONVERSATION_TYPE::PUBLIC_CHANNEL;
		}
	} else if (json.value("is_mpim", false)) {
		// It appears that is_mpim is more useful than is_group,
		// which meant private channel on older clients.
		return CONVERSATION_TYPE::GROUP_MESSAGE;
	} else if (json.value("is_im", false)) {
		return CONVERSATION_TYPE::DIRECT_MESSAGE;
	} else {
		core.alert("Unknown conversation type! Defaulting to channel.");
		if (json.value("is_private", true)) {
			return CONVERSATION_TYPE::PRIVATE_CHANNEL;
		} else {
			return CONVERSATION_TYPE::PUBLIC_CHANNEL;
		}
	}
}