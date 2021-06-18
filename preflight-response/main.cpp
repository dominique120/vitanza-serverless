#include "../common/common.h"

using namespace aws::lambda_runtime;

invocation_response my_handler(invocation_request const& request) {

	// Set up the response
	nlohmann::json response;
	response["isBase64Encoded"] = false;
	response["headers"]["Content-Type"] = "application/json";
	response["headers"]["Access-Control-Allow-Origin"] = "http://localhost:4200";
	response["headers"]["Access-Control-Allow-Credentials"] = "true";
	response["headers"]["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS";
	response["headers"]["Access-Control-Max-Age"] = 8600;
	response["headers"]["Access-Control-Allow-Headers"] = "x-vts-auth";
	response["statusCode"] = 200;

	// test to see if the payload the function got is a well formatted json
	nlohmann::json payload;
	try {
		payload = nlohmann::json::parse(request.payload);
	} catch (nlohmann::json::exception& e) {
		response["x-vts-error"] = e.what();
		response["statusCode"] = 500;
		return invocation_response::failure(response.dump(), "Payload parsing failed");
	}

	return invocation_response::success(response.dump(), "application/json");
}


int main() {
	run_handler(my_handler);
	return 0;
}