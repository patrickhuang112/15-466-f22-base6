#include "Gamel.hpp"

#include <ctime>
#include <cmath>

namespace Gamel {

float Playerl::get_y() {
    assert(!available_ys.empty());
    float res = available_ys[available_ys.size() - 1];
    available_ys.pop_back();
    return res;
}

void Playerl::return_y(float y) {
    available_ys.emplace_back(y);
}

void Playerl::add_word(const std::string& s, bool is_steal){
    words.emplace_back(std::make_shared<Wordl>(s, DEFAULT_WORD_LIFETIME, get_y()));
    std::shared_ptr<TrieNode> cur = root; 
    for (size_t i = 0; i < s.length(); ++i) {
        char c = s[i];
        auto it = cur->children.find(c);
        if (it == cur->children.end()) {
            auto new_node = std::make_shared<TrieNode>(c); 
            cur->children.emplace(c, new_node);
            cur = new_node;
        } 
        else {
            cur = it->second;
        }
        if (i == s.length() - 1) {
            cur->is_leaf = true; 
            cur->is_steal = is_steal;
            cur->word = s;
        }
    }       
};


bool Wordl::move(float elapsed) {
    if (elapsed + remaining > lifetime) {
        return false;
    }
    pos = glm::vec2(pos.x + elapsed * velo, pos.y);
    remaining += elapsed; 
    return true;
}

void Playerl::move_words(float elapsed) {
    std::list<std::shared_ptr<Wordl>> newlist;
    for (auto w : words) {
        if (w->move(elapsed)) {
            newlist.emplace_back(w); 
        }
        else {
            // ? 
        }
    }
    words = newlist; 
}

Match Playerl::match_letter(char c, std::string& completed) {
    auto curr = visited[visited.size()-1];
    auto it = curr->children.find(c);
    if (it == curr->children.end()) {
        return No; 
    }
    curr = it->second;
    visited.emplace_back(curr);
    if (!curr->is_leaf) {
        return Letter;
    }
    completed = curr->word;
    bool issteal = curr->is_steal;
    remove_word_list(visited); 
    auto it2 = words.begin();
    
    while (it2 != words.end()) {
        if ((*it2)->s ==  visited[visited.size()-1]->word) {
            words.erase(it2);
            break;
        }    
        it2++; 
    }
    visited.clear();
    visited.emplace_back(root);
    return issteal ? WordOpp : WordSelf;
}

void Playerl::process_letter(char c) {
    std::string completed = "";
    Match res = match_letter(c, completed); 
    if (res == Match::No) {
        // SFX?
    }
    else if (res == Match::Letter) {
        // SFX?
    }
    else if (res == Match::WordOpp) {
        // SFX?
        assert(completed != "");
        score += (static_cast<float>(completed.length()) * STEAL_MULTIPLIER);
    }
    else if (res == Match::WordSelf) {
        // SFX?
        assert(completed != "");
        score += (static_cast<float>(completed.length()));
    }
}

bool Playerl::has_word(const std::string& s) {
    std::shared_ptr<TrieNode> cur = root; 
    for (size_t i = 0; i < s.length(); ++i) {
        char c = s[i];
        auto it = cur->children.find(c);
        if (it == cur->children.end()) {
            return false;
        } 
        cur = it->second;
    } 
    return true;
}

void Playerl::remove_word(const std::string& s){
    std::vector<std::shared_ptr<TrieNode>> v;
    v.clear(); 
    v.emplace_back(root);
    std::shared_ptr<TrieNode> cur = root; 
    for (size_t i = 0; i < s.length(); ++i) {
        char c = s[i];
        auto it = cur->children.find(c);
        assert(it != cur->children.end());
        cur = it->second;
        v.emplace_back(cur);
    } 
    remove_word_list(v);
    auto it = words.begin();
    while (it != words.end()) {
        if ((*it)->s ==  v[v.size()-1]->word) {
            words.erase(it);
            return;
        }    
        it++; 
    }
}

void Playerl::remove_word_list(const std::vector<std::shared_ptr<TrieNode>>& wordl) {
    int cur = static_cast<int>(wordl.size()) - 2; 
    while (cur >= 0 ) {
        auto node = wordl[cur];
        auto rem = wordl[cur+1];
        if (rem->children.empty()) {
            node->children.erase(rem->c); 
        }
        --cur;
    } 
}

std::string Gamel::create_new_word() {
    double random_val = (dis(e) * std::time(0) * dis(e)) ;
    size_t mod = static_cast<size_t>(random_val) % words.size();
    const auto& it = std::next(words.begin(), mod);
    const std::string& s = *it;
    words.erase(it); 
    return s;
}
}