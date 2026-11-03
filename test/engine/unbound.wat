;; enginetool refuses this module at load: no row of the O5 binding binds
;; the import permit.nope. The module has no decide export.
(module
  (import "permit" "nope" (func (param i64) (result i64)))
  (func (result i64) i64.const 0))
