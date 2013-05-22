#!/bin/sh
# Installation script, tested on Ubuntu 12.04

# Fail on error.
set -e

START_DIR=`pwd`

echo "Installing stuff in home directory"
cd ~/
sudo apt-get install -y aptitude

echo "Installing Mininet 2.0 deps"
cd ~
git clone https://github.com/mininet/mininet.git
mininet/util/install.sh -fnv

echo "Installing Mininet via make develop"
#A development install creates symbolic links from the Python library directory into your Mininet source tree, allowing updates (e.g. via get fetch; git pull --rebase) to take effect immediately rather than requiring a separate sudo make install.
cd ~/mininet
sudo make develop
cd ..

echo "Stopping OVS-controller"
sudo service openvswitch-controller stop
sudo update-rc.d openvswitch-controller disable

echo "Installing OpenFaucet from source"
git clone git@github.com:nikhilh/openfaucet.git
cd openfaucet
./bootstrap.sh
./configure
make
sudo make install
cd ..

echo "Installing screen"
sudo apt-get install -y screen

echo "Adding additional Python deps"
sudo aptitude install -y python-twisted
sudo easy_install pexpect ipaddr termcolor

echo "Installing Wireshark + tcpdump"
sudo aptitude install -y wireshark tcpdump

echo "Installing mongodb + deps"
sudo aptitude install -y mongodb
sudo easy_install pymongo bson
MONGO_CONF=/etc/mongodb.conf
sudo sed -i 's/^bind_ip = 127.0.0.1/bind_ip = 0.0.0.0/' ${MONGO_CONF}
sudo sed -i 's/^#port = 27017/port = 27017/' ${MONGO_CONF}
sudo sed -i 's/^#noauth = true/noauth = true/' ${MONGO_CONF}
sudo service mongodb restart

echo "Installing libpcap"
sudo aptitude install -y libpcap-dev

echo "Installing dpkt from source" 
sudo aptitude install -y subversion
svn checkout http://dpkt.googlecode.com/svn/trunk/ dpkt-read-only
cd dpkt-read-only
make
sudo make install
cd ..

echo "Installing RipL/RipL-POX (optional)"
git clone git://github.com/brandonheller/ripl.git
cd ~/ripl
sudo python setup.py develop
cd ..
git clone git://github.com/brandonheller/riplpox.git
cd riplpox
sudo python setup.py develop
cd ..

echo "Installing POX"
cd ~/
git clone git://github.com/noxrepo/pox.git

echo "Installing gdb"
sudo aptitude install -y gdb

echo "Installing scons"
sudo aptitude install -y scons

echo "Installing boost"
sudo aptitude install -y libboost-all-dev

echo "Installing ZeroMQ"
sudo aptitude install -y libzmq-dev

echo "Installing libcrafter"
cd ~/
git clone https://code.google.com/p/libcrafter/
cd libcrafter/libcrafter
./autogen.sh
./configure
make
sudo make install
cd ~/

echo "Installing libpci-dev"
sudo aptitude install -y libpci-dev

echo "Installing flex"
sudo aptitude install -y flex

echo "Installing bison"
sudo aptitude install -y bison

cd $START_DIR
