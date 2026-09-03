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

-- Gate 7 (T710): one row per type-mapping row this slice adds, so NFR-002.1's
-- round-trip requirement has somewhere to round-trip to.
DROP TABLE IF EXISTS typed;
CREATE TABLE typed (
  k        SMALLINT     NOT NULL,
  i32      INTEGER      NULL,
  i64      BIGINT       NULL,
  f4       REAL         NULL,
  f8       DOUBLE PRECISION NULL,
  vc       VARCHAR(26)  NULL,
  ts       TIMESTAMP    NULL DEFAULT NULL,
  PRIMARY KEY (k)
) ENGINE=InnoDB;

-- Gate 8 (T811): one column per mappable character set, plus one whose charset
-- deliberately disagrees with what charset_family.sqlc declares, so the
-- retrieval check (ESQLC-2015) has something real to refuse.
DROP TABLE IF EXISTS charsets;
CREATE TABLE charsets (
  k       SMALLINT     NOT NULL,
  c_l1    CHAR(8)      CHARACTER SET latin1  NULL,   -- SQL/MP ISO88591 (approx)
  c_l2    CHAR(8)      CHARACTER SET latin2  NULL,   -- SQL/MP ISO88592
  v_kr    VARCHAR(10)  CHARACTER SET euckr   NULL,   -- SQL/MP KSC5601
  c_greek CHAR(8)      CHARACTER SET greek   NULL,   -- SQL/MP ISO88597
  PRIMARY KEY (k)
) ENGINE=InnoDB;
