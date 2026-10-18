#!/bin/bash

# script from Deni Bertovic
# https://denibertovic.com/posts/handling-permissions-with-docker-volumes/

# Add local user
# Either use the LOCAL_USER_ID if passed in at runtime or
# fallback

USER_ID=${LOCAL_USER_ID:-9001}

if [[ $(id -u user 2> /dev/null) != $USER_ID ]]; then
	useradd --shell /bin/bash -u $USER_ID -o -c "" -m user
	export HOME=/home/user
	export profile=$HOME/.profile
	echo "set -o vi" >> $HOME/.bashrc
	echo "source .profile" >> $HOME/.bashrc
	echo "export PATH=\${PATH}:/opt/devkitpro/devkitARM/bin/" >> $profile
	echo "export PATH=\${PATH}:/opt/devkitpro/tools/bin/" >> $profile
	echo "export DEVKITPRO=/opt/devkitpro" >> $profile
	echo "export DEVKITARM=/opt/devkitpro/devkitARM" >> $profile
	echo "export CC=clang-22" >> $profile
	echo "export CXX=clang++-22" >> $profile
fi

exec /usr/local/bin/gosu user "$@"
