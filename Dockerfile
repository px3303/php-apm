FROM php:7.4.9
COPY . /src
WORKDIR /src
RUN phpize && ./configure
RUN make && make install
CMD ["php", "-d", "extension=hcapm.so", "example.php"]
