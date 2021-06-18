#include "../common/common.h"

#include <openssl/sha.h>
#include <iomanip>

std::string hash_password(const std::string& plain_password, const std::string& salt) {
	const std::string pwd_with_salt = plain_password + salt;
	unsigned char hash[SHA512_DIGEST_LENGTH];
	SHA512_CTX sha512;
	SHA512_Init(&sha512);
	SHA512_Update(&sha512, pwd_with_salt.c_str(), pwd_with_salt.size());
	SHA512_Final(hash, &sha512);
	std::stringstream ss;
	for (const unsigned char i : hash) {
		ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(i);
	}
	return ss.str();
}

std::string generate_token(const std::string& username, const std::string& password) {
	const jwt::claim user(username);
	const jwt::claim pwd(password);

	auto token = jwt::create()
		.set_issuer("vts-serverless")
		.set_type("JWS")
		.set_payload_claim("username", user)
		.set_payload_claim("password", pwd)
		.set_issued_at(std::chrono::system_clock::now())
		.set_expires_at(std::chrono::system_clock::now() + std::chrono::seconds{ 36000 })
		.sign(jwt::algorithm::hs512{ "9E590B6B427FB984C1CC5F5E0373BB163A3DD9D3C371CAEF3726D68CB3B894341E3758430154A878DE6D2E639A54B2C783365BC4219EAB2B69D490D0793D967A" });

	return token;
}

using namespace aws::lambda_runtime;

invocation_response my_handler(invocation_request const& request) {

	// test to see if the payload the function got is a well formatted json
	nlohmann::json payload;
	try {
		payload = nlohmann::json::parse(request.payload);
	} catch (nlohmann::json::exception& e) {
		return invocation_response::failure(e.what(), "Payload parsing failed");
	}

	nlohmann::json response;
	response["isBase64Encoded"] = false;
	response["headers"]["Content-Type"] = "application/json";
	response["headers"]["Access-Control-Allow-Origin"] = "http://localhost:4200";
	response["headers"]["Access-Control-Allow-Credentials"] = "true";
	response["headers"]["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS";
	response["headers"]["Access-Control-Max-Age"] = 8600;
	response["headers"]["Access-Control-Allow-Headers"] = "x-vts-auth";

	Aws::SDKOptions options;

#ifdef LOGGING
	options.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Trace;
	options.loggingOptions.logger_create_fn = GetConsoleLoggerFactory();
#endif // LOGGING

	Aws::InitAPI(options);


	nlohmann::json body = nlohmann::json::parse(payload.at("body").get<std::string>());

	std::string usr = body.at("username").get<std::string>();
	std::string pwd = body.at("password").get<std::string>();
	std::string hashed_pwd = hash_password(pwd, usr);

	
	nlohmann::json values;
	values["username"] = usr;

	// Run operation 
	nlohmann::json result;
	alddb::DynamoDB::query_with_expression(ddbcli(), "users", "username", "username = :username", values, result);

	try {
		std::map<std::string, std::string> user = result[0];

		bool valid_usr = false;
		bool valid_pwd = false;

		if (user.at("username") == usr) {
			valid_usr = true;
		}

		if (user.at("password") == hashed_pwd) {
			valid_pwd = true;
		}

		if (valid_usr && valid_pwd) {
			response["statusCode"] = 200;

			nlohmann::json jwt;
			jwt["jwt"] = generate_token(usr, pwd);
			response["body"] = jwt.dump();

		} else {
			response["statusCode"] = 403;
		}
	} catch (const nlohmann::json::exception& ex) {
		response["message"] = ex.what();
		response["statusCode"] = 400;
	}
	
	return invocation_response::success(response.dump(), "application/json");
}


int main() {
	run_handler(my_handler);
	return 0;
}