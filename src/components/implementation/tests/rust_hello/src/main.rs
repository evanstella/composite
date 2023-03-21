extern crate unwinding;

use std::collections::HashMap;

fn main() {
    let mut scores = HashMap::new();

    scores.insert(String::from("Blue"), 10);
    scores.insert(String::from("Yellow"), 50);
    
    get_score(scores, String::from("Yellow"));
}

fn get_score(scores: HashMap<String, i64>, team_name: String) {
    let score = match scores.get(&team_name) {
                    Some(num) => num.to_string(),
                    None => "nonexistent".to_string()
                };
    println!("{}'s score is {}", team_name, score);
}
