-- Gate 1 schema, extended for Gate 2 (T200).
-- `weight` is nullable so retrieval of a null can be exercised.
DROP TABLE IF EXISTS parts;
CREATE TABLE parts (
  part_num  SMALLINT     NOT NULL,
  part_desc CHAR(18)     NOT NULL,
  weight    SMALLINT     NULL,
  PRIMARY KEY (part_num)
) ENGINE=InnoDB;

-- Gate 5 (T515): a second table so a join makes num_tables 2 and populates
-- stats[1]. Without this every SQLSA fixture would exercise stats[0] only,
-- which cannot distinguish an array from a single struct.
DROP TABLE IF EXISTS suppliers;
CREATE TABLE suppliers (
  part_num   SMALLINT    NOT NULL,
  supp_name  CHAR(18)    NOT NULL,
  PRIMARY KEY (part_num)
) ENGINE=InnoDB;
