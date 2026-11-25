CREATE DATABASE IF NOT EXISTS `student_management`
  CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE `student_management`;

CREATE TABLE IF NOT EXISTS students (
  id INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  student_no VARCHAR(30) NOT NULL UNIQUE,
  name VARCHAR(50) NOT NULL,
  gender VARCHAR(10) NOT NULL DEFAULT '未知',
  age TINYINT UNSIGNED NOT NULL,
  major VARCHAR(100) NOT NULL DEFAULT '',
  phone VARCHAR(30) NOT NULL DEFAULT '',
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS users (
  id INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  username VARCHAR(30) NOT NULL UNIQUE,
  password_hash CHAR(64) NOT NULL,
  role ENUM('admin', 'teacher', 'viewer') NOT NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Local teaching accounts. The application also creates them automatically.
INSERT INTO users (username, password_hash, role) VALUES
  ('admin', SHA2('admin123', 256), 'admin'),
  ('teacher', SHA2('teacher123', 256), 'teacher'),
  ('viewer', SHA2('viewer123', 256), 'viewer')
ON DUPLICATE KEY UPDATE username = VALUES(username);
