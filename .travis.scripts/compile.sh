#!/bin/bash
sudo apt-get -y install check pkg-config

phpize
./configure
make all 
sudo make install
sudo echo "extension=php_ext_uv.so" >> `php --ini | grep "Loaded Configuration" | sed -e "s|.*:\s*||"`
