
# When executing cmake command set "install_prefix" to an existing directory
# to install the runtimes cmake files.

# Build lambda runtime
cd ~ 
git clone https://github.com/awslabs/aws-lambda-cpp.git
cd aws-lambda-cpp
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=~/lambda-install ..
make
make install

# Set "install_prefix" to the location where the runtime was installed to.

# Build lambda function
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=~/lambda-install ..
make
make aws-lambda-package-(package-name)