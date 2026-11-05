//! G2: a budget with costs and a split (g2-budget.lang).
//!
//! split b n gives the n units first and the rest second, and spend b n
//! gives the rest. A cost that is more than the budget gives none.
#[derive(Clone, Copy)]
struct Budget(u64);

fn split(b: Budget, n: u64) -> Option<(Budget, Budget)> {
    b.0.checked_sub(n).map(|r| (Budget(n), Budget(r)))
}

fn spend(b: Budget, n: u64) -> Option<Budget> {
    b.0.checked_sub(n).map(Budget)
}

fn budget_left(b: Budget) -> u64 {
    b.0
}

/// A constructor application in the langc normal form.
fn app(head: &str, args: &[String]) -> String {
    args.iter().fold(head.to_string(), |s, a| if a.contains(' ') { format!("{s} ({a})") } else { format!("{s} {a}") })
}

fn show_budget(b: &Budget) -> String {
    app("budget", &[b.0.to_string()])
}

fn show_option<T>(value: &Option<T>, show: impl Fn(&T) -> String) -> String {
    value.as_ref().map_or("none".to_string(), |x| app("some", &[show(x)]))
}

fn main() {
    let halves = split(Budget(5), 3);
    let rest = spend(Budget(5), 3);
    let left = budget_left(Budget(4));
    println!("{}", show_option(&halves, |(a, b)| app("pair", &[show_budget(a), show_budget(b)])));
    println!("{}", show_option(&rest, show_budget));
    println!("{left}");
}
