#ifndef MATTERMOST_ACCOUNT
#define MATTERMOST_ACCOUNT

#include <string>
#include <map>
#include "include/IProtocolSession.h"
#include "include/ICore.h"
#include "include/ITeam.h"
#include <nlohmann/json.hpp>

using namespace Polychat;

namespace Polychat {
class IAccount;
}

namespace PolySlackPlugin {
class PolySlack;

/**
 * Represents a user on Slack. Can be the logged in user
 * or just another team member.
 */
class SlackAccountSession : public Polychat::IProtocolSession {
private:
	IAccount& coreAccount;

	// For logged in accounts
	std::string token = "";
	bool tokenIsValid = false;
	std::string uid = "";
	std::string teamID = "";

	std::shared_ptr<IHTTPClient> webClient;

	Polychat::ICore& core;

	// The websocket that is used for realtime updates.
	// NOTE: This is per account because the servers
	// only support 1 account per connection.
	std::shared_ptr<Polychat::IWebSocket> webSocketConnection;

	// Mapping events to the functions that handle them
	std::map<std::string, std::function<void(SlackAccountSession*, nlohmann::json)>> eventMap;

	void loadEventFunctions();
	void onMessageEvent(nlohmann::json json);

	/**
	 * Update the teams.
	 */
	void updateTeams(bool updateConversations);

	void updateConversations(std::shared_ptr<ITeam> team);

	CONVERSATION_TYPE getTypeFromJSON(nlohmann::json& json);

	void initWebsockets();

	void onWSMessageReceived(std::string);
	void onWSOpen();
	void onWSClose();
public:
	SlackAccountSession(Polychat::IAccount& coreAccount, std::string token, std::string uid, Polychat::ICore& core);

	void setToken(std::string token) {
		this->token = token;
		tokenIsValid = true;
	}

	std::string getToken() {
		return token;
	}

	virtual IAccount& getAccount() {
		return coreAccount;
	}

	void getFullUserDetails();

	virtual void refresh(std::shared_ptr<IConversation> currentlyViewedConversation);

	virtual void updatePosts(IConversation& conversation, int limit);

	virtual bool isValid();

	virtual void sendMessageAction(std::shared_ptr<Message>, MessageAction);
};

#endif // !POLYSLACK_ACCOUNT

}