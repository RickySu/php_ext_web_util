#!/bin/bash
sudo apt-get -y install check pkg-config libpcre3 libpcre3-dev
phpize
./configure
make thirdparty/build/lib/libr3.a thirdparty/build/lib/libhttp_parser.o
make all 
#sudo make install
#sudo echo "extension=php_ext_uv.so" >> `php --ini | grep "Loaded Configuration" | sed -e "s|.*:\s*||"`
