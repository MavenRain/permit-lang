// permit-lang T3: the static scope of def start as Cedar permit policies.
// Cedar denies each request that no policy permits; principal is open.
// Each Resource::"a/b" must have the parent Resource::"a" in the request entities.
// The start scope has no grant: no policy.
// Not exported: steps. def start allows 2 calls.
// Not exported: budget. def start has 5 units; the gate takes the cost of each call.
// Not exported: approval binding. Each execute call needs an approval with the digest of its canonical target and payload, used one time.
// Not exported: sequence guard (L5). On each Plan path, an execute comes after an approved propose of the same mutation.
// Not exported: laws. none
// Not exported: bind lines. "re\tad" inspect "/path"
