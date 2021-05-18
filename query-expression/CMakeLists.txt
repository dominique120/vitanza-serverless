cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 11)

project(query-expression LANGUAGES CXX)

find_package(aws-lambda-runtime REQUIRED)
find_package(AWSSDK COMPONENTS core dynamodb)
find_package(dynamodb-cpp REQUIRED)

add_executable(${PROJECT_NAME} "main.cpp")
target_link_libraries(${PROJECT_NAME} 
	PUBLIC 
	AWS::aws-lambda-runtime 
	${AWSSDK_LINK_LIBRARIES} 
	dynamodb-cpp::dynamodb-cpp)

aws_lambda_package_target(${PROJECT_NAME})