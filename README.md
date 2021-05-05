# audiows

## Build and test
docker build -t asterisk-audiows-buildenv .

docker run --rm -it --net=host -v "$(pwd)/app_audiows.c:/asterisk/apps/app_audiows.c" -v "$(pwd)/test/etc-asterisk:/etc/asterisk" asterisk-audiows-buildenv bash

make && make install

asterisk -cvvv