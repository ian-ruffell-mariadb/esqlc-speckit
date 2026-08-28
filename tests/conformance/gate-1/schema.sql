-- Gate 1 schema, extended for Gate 2 (T200).
-- `weight` is nullable so retrieval of a null can be exercised.
DROP TABLE IF EXISTS parts;
CREATE TABLE parts (
  part_num  SMALLINT     NOT NULL,
  part_desc CHAR(18)     NOT NULL,
  weight    SMALLINT     NULL,
  PRIMARY KEY (part_num)
) ENGINE=InnoDB;
