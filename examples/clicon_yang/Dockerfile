FROM ubuntu:14.04
# 12.04
MAINTAINER Olof Hagsand <olof@hagsand.se>
ENV DEBIAN_FRONTEND noninteractive
RUN apt-get update && apt-get install -y \
    libqdbm-dev     \
    curl
COPY libcligen.so.3.5 /usr/lib/
COPY libclicon.so.3.0 /usr/lib/
COPY libclicon_cli.so.3.0 /usr/lib/
COPY clicon_cli /usr/bin/
COPY ietf-inet-types.yang /usr/local/share/clicon_yang/yang/
COPY clicon_yang.yang /usr/local/share/clicon_yang/yang/
COPY clicon_yang_cli.cli /usr/local/lib/clicon_yang/clispec/
COPY clicon_yang.conf /usr/local/etc/
COPY nullfile /usr/local/var/clicon_yang/
COPY start.sh /usr/bin/
RUN ldconfig
#CMD ["clicon_cli", "-c", "-f", "/usr/local/etc/clicon_yang.conf"]
CMD ["start.sh"]

EXPOSE 7878 7878/udp






