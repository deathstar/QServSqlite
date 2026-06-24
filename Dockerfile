# Stage 1: Build
FROM ubuntu:22.04 AS builder
RUN apt-get update && apt-get install -y build-essential cmake libenet-dev zlib1g-dev libsqlite3-dev
WORKDIR /src
COPY . .
RUN cmake . && make

# Stage 2: Runtime
FROM ubuntu:22.04
RUN apt-get update && apt-get install -y libenet-dev zlib1g-dev libsqlite3-dev && rm -rf /var/lib/apt/lists/*
RUN useradd -ms /bin/bash qservuser
USER qservuser

WORKDIR /QServSqlite

COPY --from=builder /src/qserv /QServSqlite/qserv 
EXPOSE 28785/udp
EXPOSE 28786/udp
CMD ["./qserv"]
