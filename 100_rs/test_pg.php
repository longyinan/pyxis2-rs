<?php

$conn = pg_connect("host=pg18 port=5432 dbname=testdb user=testuser password=testpass");

if (!$conn) {
    die("connect failed: " . pg_last_error());
}

$result = pg_query($conn, "SELECT version()");
$row = pg_fetch_row($result);

echo "<pre>";
echo "Connected OK\n";
echo $row[0];
echo "</pre>";