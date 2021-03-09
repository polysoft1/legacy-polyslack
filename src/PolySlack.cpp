#ifndef POLYMOST
#define POLYMOST

#include "PolySlack.h"
#include "SlackAccountSession.h"

#include "include/IWebHelper.h"
#include "include/IAccountManager.h"

#include <nlohmann/json.hpp>

#include <memory>
#include <string>
#include <iostream>

const std::string SLACK_URL = "slack.com";

using namespace Polychat;
using namespace PolySlackPlugin;

std::string PolySlack::getPluginName() const {
	return "PolySlack";
}
std::string PolySlack::getProtocolName() const {
	return "slack";
}

PolySlack::PolySlack() {
	loginFieldsList.push_back(LoginField("token", true, false, true));
}

PolySlack::~PolySlack() {

}

bool PolySlack::initialize(ICore* core) {
	this->core = core;

	return true;
}

std::string PolySlack::getDatabaseName() const {
	return "polyslack";
}

AuthStatus PolySlack::login(std::map<std::string, std::string> fields, IAccount& account) {
	std::string token = fields["token"];

	//nlohmann::json json;
	//std::string contentString = json.dump();

	std::shared_ptr<HTTPStringContent> content = std::make_shared<HTTPStringContent>("");
	content->setContentType("application/x-www-form-urlencoded");

	std::string url = "/api/auth.test";
	HTTPMessage message(HTTPMethod::GET, url);
	message.setContent(content);
	message.setAuthorization("Bearer", token);
	
	IWebHelper& web = core->getWebHelper();

	std::shared_ptr<IHTTPClient> webClient = web.initHTTPClient(SLACK_URL, 443, true);
	webClient->sendRequest(message, [this, token, &account](std::shared_ptr<HTTPMessage> responsePtr) {
		if (responsePtr->getStatus() == HTTPStatus::HTTP_OK) {
			HTTPMessage& response = *responsePtr.get();
			auto userObj = nlohmann::json::parse(response.getContent()->getAsString());
			bool isOkay = userObj["ok"];
			if (isOkay == true) {
				std::string userID = userObj["user_id"];
				std::string user = userObj["user"];
				std::string team = userObj["team"];
				account.setUID(userID);
				account.setUsername(user);

				auto session = sessions.emplace(std::make_shared<SlackAccountSession>(account, token, userID, *core));
				account.setSession(*session.first);

				core->getAccountManager().alertOfSessionChange(account, AuthStatus::AUTHENTICATED);
			} else {
				core->getAccountManager().alertOfSessionChange(account, AuthStatus::FAIL_OTHER);
				core->alert(userObj["error"]);
			}
		} else {
			core->getAccountManager().alertOfSessionChange(account, AuthStatus::FAIL_HTTP_ERROR);
			core->alert(std::to_string((int)responsePtr->getStatus()));
		}
		}
	);

	
	return AuthStatus::CONNECTING;
}

extern "C" {
#ifdef _WIN32
	__declspec(dllexport) PolySlack* create()
	{
		return new PolySlack;
	}

	__declspec(dllexport) void destroy(PolySlack * in)
	{
		delete in;
	}
#else
	PolySlack* create()
	{
		return new PolySlack;
	}

	void destroy(PolySlack* in)
	{
		delete in;
	}
#endif
}
#endif
