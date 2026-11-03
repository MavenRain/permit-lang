;; enginetool refuses this module at load: the O5 row of permit.hash has
;; 1 parameter and this import has 2.
(module
  (import "permit" "hash" (func (param i64 i64) (result i64)))
  (func (result i64) i64.const 0))
