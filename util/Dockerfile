FROM debian:13

RUN apt update -y

###############################################################################
# Install gosu

RUN apt install -y curl wget
RUN curl -o /usr/local/bin/gosu -SL "https://github.com/tianon/gosu/releases/download/1.10/gosu-amd64"
RUN chmod +x /usr/local/bin/gosu

###############################################################################
# Install dev tools

RUN sudo apt-get install -y \
	build-essential \
	ccache \
	cmake \
	make \
	ninja-build \
	python3 \
	qt6-base-dev \
	qt6-websockets-dev

###############################################################################
# Setup sudoers

ADD devenv/sudoers /etc/sudoers

###############################################################################
# Setup working directory

RUN mkdir /usr/src/project
WORKDIR /usr/src/project

###############################################################################
# Setup entrypoint

ADD devenv/entrypoint.sh /
ENTRYPOINT ["/entrypoint.sh"]
