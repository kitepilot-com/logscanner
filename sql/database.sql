CREATE DATABASE logscanner;
CREATE USER 'logscanner_admin'@'localhost' IDENTIFIED BY 'YOUR-PASSWORD-HERE';
GRANT ALL PRIVILEGES ON logscanner.* TO 'logscanner_admin'@'localhost';
CREATE USER 'logscanner_user'@'localhost' IDENTIFIED BY 'U5erP455W0rd!';
GRANT SELECT, INSERT, UPDATE, DELETE ON logscanner.* TO 'logscanner_user'@'localhost';
FLUSH PRIVILEGES;
