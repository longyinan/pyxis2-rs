FROM php:8.3-cli

WORKDIR /app
COPY index.php /app/

ENV PORT=8080
CMD sh -c "php -S 0.0.0.0:${PORT} -t /app"