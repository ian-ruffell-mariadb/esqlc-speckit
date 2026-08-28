-- T008 — Gate 1 schema fixture. Two columns, two type families, no more.
DROP TABLE IF EXISTS parts;
CREATE TABLE parts (
  part_num  SMALLINT     NOT NULL,
  part_desc CHAR(18)     NOT NULL,
  PRIMARY KEY (part_num)
) ENGINE=InnoDB;
