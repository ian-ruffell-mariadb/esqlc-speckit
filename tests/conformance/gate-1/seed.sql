-- Gate 2 seed, extended for Gate 3 (T300).
-- 4102 has a weight; 4103's weight is null; 4104-4106 give a cursor a range.
-- part_desc values are exactly 18 bytes and end in blanks, exercising DIV-052.
DELETE FROM parts;
INSERT INTO parts (part_num, part_desc, weight) VALUES (4102, 'HEX NUT, 8MM      ', 42);
INSERT INTO parts (part_num, part_desc, weight) VALUES (4103, 'WASHER, 8MM       ', NULL);
INSERT INTO parts (part_num, part_desc, weight) VALUES (4104, 'BOLT, 8MM X 40    ', 95);
INSERT INTO parts (part_num, part_desc, weight) VALUES (4105, 'SPRING WASHER, 8MM', 12);
INSERT INTO parts (part_num, part_desc, weight) VALUES (4106, 'CAP NUT, 8MM      ', 51);

-- Gate 5 (T515): supplier rows for the two-table join.
DELETE FROM suppliers;
INSERT INTO suppliers (part_num, supp_name) VALUES
  (3103, 'ACME SUPPLY CO   '),
  (3201, 'BOLT WORKS LTD   '),
  (4102, 'COGSWELL COGS    ');
