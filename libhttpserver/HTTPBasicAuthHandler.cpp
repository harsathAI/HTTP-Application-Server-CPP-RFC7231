#include "HTTPBasicAuthHandler.hpp"
#include "nlohmann/json.hpp"
#include "internal/base64.hpp"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>

#define str6_cmp(m, c0, c1, c2, c3, c4, c5)		\
	*(m)==c0 && *(m+1)==c1 && *(m+2)==c2 && *(m+3)==c3 && *(m+4)==c4 && *(m+5)==c5

HTTP::BasicAuth::BasicAuthHandler::BasicAuthHandler(const std::string& cred_file)
	: _auth_cred_filename{cred_file} {
	m_populate_map();
	}

void HTTP::BasicAuth::BasicAuthHandler::m_populate_map(){
	std::ifstream cred_stream{_auth_cred_filename};
	std::string file_data;
	if(!cred_stream.is_open()){
	 	perror("Unable to open API auth file for REST services\n");
		::exit(1);
	}
	std::string line;
	while(std::getline(cred_stream, line)){
		file_data.append(line);
	}
	cred_stream.close();
	auto json_reader = nlohmann::json::parse(file_data);
	for(const auto& endpoints : json_reader.items()){
		for(const auto& credentials : endpoints.value().items()){
			_endpoint_cred_map[endpoints.key()].emplace_back(credentials.key(), credentials.value());
		}
	}
}

bool HTTP::BasicAuth::BasicAuthHandler::check_credentials(
		const std::string &endpoint, const std::string &encoded_auth_param){
	// Lets parse the user-id:password from the base64 encoded user-agent request;
	std::optional<std::pair<std::string, std::string>> parsed_results = 
		m_basic_auth_cred_parser(base64::b64decode(
			reinterpret_cast<const unsigned char*>(encoded_auth_param.c_str()), encoded_auth_param.size()
				));
	if(!parsed_results.has_value()){ return false; }
	// user-agent sent a User:Pass even though the endpoint does not need authorization
	if(!_endpoint_cred_map.contains(endpoint)){ return true; }
	const std::vector<std::pair<std::string, std::string>>& user_creds = _endpoint_cred_map.at(endpoint);
	auto result = std::find_if(std::begin(user_creds), std::end(user_creds), 
			[&parsed_results](const std::pair<std::string, std::string>& values) -> bool {
			    return ((values.first == parsed_results.value().first) && 
					BCrypt::validatePassword(parsed_results.value().second, values.second));
			});
	if(result == std::end(user_creds)){ return false; }
	return true;
}

std::optional<std::pair<std::string, std::string>> 
HTTP::BasicAuth::BasicAuthHandler::m_basic_auth_cred_parser(const std::string& decoded_auth_param){
	/* basic-credentials	= base64-user-pass
	 * base64-user-pass	= <base64 encoding of user-pass,
	 * 			   except not limited to 79 char/line>
	 * user-pass		= userid ":" password
	 * userid		= *<TEXT excluding ":">
	 * password		= *TEXT
	 */
	enum class AuthParserStates : std::uint8_t { 
		USERID_BEGIN = 0,
		USERID_COLON,
		PASSWORD_BEGIN,
		PARSER_END,
		PARSER_ERROR
	};
	std::string::const_iterator auth_param_iter = decoded_auth_param.begin();
	std::size_t parsed_bytes{};
	bool parsing_done{false};
	bool parsed_successfully{false};
	auto next_character = [&auth_param_iter, &parsed_bytes](void) -> void {
		auth_param_iter++;
		parsed_bytes++;
	};
	// initial state
	AuthParserStates State = AuthParserStates::USERID_BEGIN;
	std::string parsed_username;
	std::string parsed_password;
	while(!parsing_done){
		switch(State){
			case AuthParserStates::USERID_BEGIN:
				{
					if(!is_control_character(*auth_param_iter) && (*auth_param_iter != ':')){
						parsed_username.push_back(*auth_param_iter);					
						next_character();
					}else if(*auth_param_iter == ':'){
						State = AuthParserStates::USERID_COLON;
					}else{
						State = AuthParserStates::PARSER_ERROR;
					}
					break;
				}
			case AuthParserStates::USERID_COLON:
				{
					if(*auth_param_iter == ':'){
						State = AuthParserStates::PASSWORD_BEGIN;
						next_character();
					}else{
						State = AuthParserStates::PARSER_ERROR;
					}
					break;
				}
			case AuthParserStates::PASSWORD_BEGIN:
				{
					if(!is_control_character(*auth_param_iter) && (*auth_param_iter != '\0')){
						parsed_password.push_back(*auth_param_iter);	
						next_character();
					}else if(*auth_param_iter == '\0'){
						State = AuthParserStates::PARSER_END;
					}else{
						State = AuthParserStates::PARSER_ERROR;
					}
					break;
				}
			case AuthParserStates::PARSER_END:
				{
					parsing_done = true;	
					parsed_successfully = true;
					break;
				}
			case AuthParserStates::PARSER_ERROR:
				{
					parsing_done = true;
					parsed_successfully = false;
					break;
				}
		}
	}
	return parsed_successfully ? 
		std::optional<std::pair<std::string, std::string>>{ {std::move(parsed_username), std::move(parsed_password)} } 
		: std::nullopt;
}

bool HTTP::BasicAuth::is_control_character(const unsigned char value){
	return (((value > static_cast<unsigned char>(0x1F)) && (value <= static_cast<unsigned char>(0x00)) && 
			value != static_cast<unsigned char>(0x7F)));
}

std::optional<std::string> HTTP::BasicAuth::split_base64_from_scheme(const std::string& header_value){
	if(!(str6_cmp(header_value.c_str(), 'B', 'a', 's', 'i', 'c', ' '))){ return std::nullopt; }
	std::string returner;	
	const char* base64_value = (header_value.c_str()+6);
	while(*base64_value != '\0'){
		returner.push_back(*base64_value);
		base64_value++;
	}
	return returner;
}
