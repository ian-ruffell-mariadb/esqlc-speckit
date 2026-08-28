-- Gate 2 seed (T200). Row 4102 has a weight; row 4103 has a null weight.
-- part_desc values are exactly 18 bytes and end in blanks, so DIV-052 is
-- exercised by criterion 3 rather than assumed.
DELETE FROM parts;
INSERT INTO parts (part_num, part_desc, weight) VALUES (4102, 'HEX NUT, 8MM      ', 42);
INSERT INTO parts (part_num, part_desc, weight) VALUES (4103, 'WASHER, 8MM       ', NULL);
