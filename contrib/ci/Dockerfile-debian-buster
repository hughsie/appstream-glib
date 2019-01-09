FROM debian:buster

RUN echo "deb-src http://deb.debian.org/debian/ buster main" >> /etc/apt/sources.list
RUN apt-get update -qq
RUN apt-get install -yq --no-install-recommends meson libstemmer-dev
RUN apt-get build-dep -yq appstream-glib

RUN mkdir /build
WORKDIR /build
