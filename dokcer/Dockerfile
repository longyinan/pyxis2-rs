FROM seti/php53

COPY ./dokcer/php.ini /etc/php.ini
COPY ./dokcer/000-default.conf /etc/apache2/sites-available/000-default.conf

RUN cp -f /etc/php.ini /etc/php53/apache2/php.ini \
 && cp -f /etc/php.ini /etc/php53/cli/php.ini \
 && apt-get update \
 && apt-get install -y php53-mod-mcrypt php53-mod-pgsql
