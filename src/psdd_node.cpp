//
// Created by Yujia Shen on 10/19/17.
//

#include "psdd_node.h"
#include <algorithm>
#include <functional>
#include <cassert>
#include <unordered_set>
#include <stack>
#include <queue>
#include <random>
#include <cmath>
#include <iostream>
namespace vtree_util {
std::vector<Vtree *> SerializeVtree(Vtree *root) {
  std::vector<Vtree *> serialized_vtree;
  std::queue<Vtree *> vtree_queue;
  vtree_queue.push(root);
  while (!vtree_queue.empty()) {
    Vtree *front_node = vtree_queue.front();
    vtree_queue.pop();
    serialized_vtree.push_back(front_node);
    if (!sdd_vtree_is_leaf(front_node)) {
      vtree_queue.push(sdd_vtree_left(front_node));
      vtree_queue.push(sdd_vtree_right(front_node));
    }
  }
  return serialized_vtree;
}
}
namespace psdd_node_util {

SddNode *ConvertPsddNodeToSddNode(const std::vector<PsddNode *> &serialized_psdd_nodes, SddManager *sdd_manager) {
  for (auto node_it = serialized_psdd_nodes.rbegin(); node_it != serialized_psdd_nodes.rend(); ++node_it) {
    PsddNode *cur_node = *node_it;
    if (cur_node->node_type() == LITERAL_NODE_TYPE) {
      PsddLiteralNode *cur_literal = cur_node->psdd_literal_node();
      SddNode *cur_lit = sdd_manager_literal(cur_literal->literal(), sdd_manager);
      cur_node->SetUserData((uintmax_t) cur_lit);
    } else if (cur_node->node_type() == DECISION_NODE_TYPE) {
      PsddDecisionNode *cur_decn_node = cur_node->psdd_decision_node();
      const auto &decn_primes = cur_decn_node->primes();
      const auto &decn_subs = cur_decn_node->subs();
      auto element_size = decn_primes.size();
      SddNode *cur_logic = sdd_manager_false(sdd_manager);
      for (auto i = 0; i < element_size; ++i) {
        PsddNode *cur_prime = decn_primes[i];
        PsddNode *cur_sub = decn_subs[i];
        SddNode *cur_partition =
            sdd_conjoin((SddNode *) cur_prime->user_data(), (SddNode *) cur_sub->user_data(), sdd_manager);
        cur_logic = sdd_disjoin(cur_logic, cur_partition, sdd_manager);
      }
      cur_node->SetUserData((uintmax_t) cur_logic);
    } else {
      assert(cur_node->node_type() == TOP_NODE_TYPE);
      PsddTopNode *cur_top = cur_node->psdd_top_node();
      cur_node->SetUserData((uintmax_t) sdd_manager_true(sdd_manager));
    }
  }
  auto root_logic = (SddNode *) serialized_psdd_nodes[0]->user_data();
  for (PsddNode *cur_node : serialized_psdd_nodes) {
    cur_node->SetUserData(0);
  }
  return root_logic;
}

// parents appear before children
std::vector<PsddNode *> SerializePsddNodes(PsddNode *root) {
  return std::move(SerializePsddNodes(std::vector<PsddNode *>({root})));
}

std::vector<PsddNode *> SerializePsddNodes(const std::vector<PsddNode *> &root_nodes) {
  std::unordered_set<uintmax_t> node_explored;
  std::vector<PsddNode *> result;
  for (const auto cur_root_node : root_nodes) {
    if (node_explored.find(cur_root_node->node_index()) == node_explored.end()) {
      result.push_back(cur_root_node);
      node_explored.insert(cur_root_node->node_index());
    }
  }
  uintmax_t explore_index = 0;
  while (explore_index != result.size()) {
    PsddNode *cur_psdd_node = result[explore_index];
    if (cur_psdd_node->node_type() == 2) {
      auto cur_decn_node = static_cast<PsddDecisionNode *>(cur_psdd_node);
      const std::vector<PsddNode *> &primes = cur_decn_node->primes();
      const std::vector<PsddNode *> &subs = cur_decn_node->subs();
      for (const auto cur_prime : primes) {
        if (node_explored.find(cur_prime->node_index()) == node_explored.end()) {
          node_explored.insert(cur_prime->node_index());
          result.push_back(cur_prime);
        }
      }
      for (const auto cur_sub : subs) {
        if (node_explored.find(cur_sub->node_index()) == node_explored.end()) {
          node_explored.insert(cur_sub->node_index());
          result.push_back(cur_sub);
        }
      }
    }
    ++explore_index;
  }
  return std::move(result);
}

std::unordered_map<uintmax_t, PsddNode *> GetCoveredPsddNodes(const std::vector<PsddNode *> &root_nodes) {
  std::unordered_map<uintmax_t, PsddNode *> covered_nodes;
  std::queue<PsddNode *> front_nodes;
  for (const auto cur_root_node : root_nodes) {
    if (covered_nodes.find(cur_root_node->node_index()) == covered_nodes.end()) {
      front_nodes.push(cur_root_node);
      covered_nodes[cur_root_node->node_index()] = cur_root_node;
    }
  }
  while (!front_nodes.empty()) {
    PsddNode *cur_psdd_node = front_nodes.front();
    front_nodes.pop();
    if (cur_psdd_node->node_type() == 2) {
      auto cur_decn_node = static_cast<PsddDecisionNode *>(cur_psdd_node);
      const std::vector<PsddNode *> &primes = cur_decn_node->primes();
      const std::vector<PsddNode *> &subs = cur_decn_node->subs();
      for (const auto cur_prime : primes) {
        if (covered_nodes.find(cur_prime->node_index()) == covered_nodes.end()) {
          covered_nodes[cur_prime->node_index()] = cur_prime;
          front_nodes.push(cur_prime);
        }
      }
      for (const auto cur_sub : subs) {
        if (covered_nodes.find(cur_sub->node_index()) == covered_nodes.end()) {
          covered_nodes[cur_sub->node_index()] = cur_sub;
          front_nodes.push(cur_sub);
        }
      }
    }
  }
  return std::move(covered_nodes);
}
void SetActivationFlag(const std::bitset<MAX_VAR> &evidence, const std::vector<PsddNode *> &serialized_psdd_nodes) {
  for (auto node_it = serialized_psdd_nodes.rbegin(); node_it != serialized_psdd_nodes.rend(); ++node_it) {
    PsddNode *cur_node = *node_it;
    if (cur_node->node_type() == LITERAL_NODE_TYPE) {
      //literal
      auto cur_literal_node = cur_node->psdd_literal_node();
      if (evidence[cur_literal_node->variable_index()] == cur_literal_node->sign()) {
        cur_literal_node->SetActivationFlag();
      }
    } else if (cur_node->node_type() == DECISION_NODE_TYPE) {
      auto cur_decn_node = cur_node->psdd_decision_node();
      const auto &cur_primes = cur_decn_node->primes();
      const auto &cur_subs = cur_decn_node->subs();
      assert(cur_primes.size() == cur_subs.size());
      auto element_size = cur_primes.size();
      for (auto k = 0; k < element_size; ++k) {
        if (cur_primes[k]->activation_flag() && cur_subs[k]->activation_flag()) {
          cur_decn_node->SetActivationFlag();
          break;
        }
      }
    } else {
      // cur_node->node_type() == 3 (TOP node)
      cur_node->SetActivationFlag();
    }
  }
}

std::pair<std::bitset<MAX_VAR>, Probability> GetMPESolution(const std::vector<PsddNode *> &serialized_psdd_nodes) {
  for (auto it = serialized_psdd_nodes.rbegin(); it != serialized_psdd_nodes.rend(); ++it) {
    PsddNode *cur_node = *it;
    if (cur_node->node_type() == LITERAL_NODE_TYPE) {
      PsddLiteralNode *cur_lit = cur_node->psdd_literal_node();
      auto *cache_pair = new std::pair<PsddParameter, uintmax_t>(PsddParameter::CreateFromDecimal(1.0), 0);
      cur_node->SetUserData((uintmax_t) cache_pair);
    } else if (cur_node->node_type() == TOP_NODE_TYPE) {
      PsddTopNode *cur_top_node = cur_node->psdd_top_node();
      Probability cur_value;
      std::pair<PsddParameter, uintmax_t> *cache_pair;
      if (cur_top_node->true_parameter() > cur_top_node->false_parameter()) {
        cache_pair = new std::pair<PsddParameter, uintmax_t>(cur_top_node->true_parameter(), 1);
      } else {
        cache_pair = new std::pair<PsddParameter, uintmax_t>(cur_top_node->false_parameter(), 0);
      }
      cur_node->SetUserData((uintmax_t) cache_pair);
    } else {
      assert(cur_node->node_type() == DECISION_NODE_TYPE);
      PsddDecisionNode *cur_decn_node = cur_node->psdd_decision_node();
      Probability max_product = Probability::CreateFromDecimal(0);
      uintmax_t max_index = 0;
      const auto &cur_primes = cur_decn_node->primes();
      const auto &cur_subs = cur_decn_node->subs();
      const auto &cur_params = cur_decn_node->parameters();
      uintmax_t element_size = cur_decn_node->primes().size();
      for (uintmax_t i = 0; i < element_size; ++i) {
        PsddNode *cur_prime_node = cur_primes[i];
        PsddNode *cur_sub_node = cur_subs[i];
        PsddParameter cur_parameter = cur_params[i];
        auto cur_prime_comp_cache = (std::pair<PsddParameter, uintmax_t> *) cur_prime_node->user_data();
        auto cur_sub_comp_cache = (std::pair<PsddParameter, uintmax_t> *) cur_sub_node->user_data();
        Probability cur_product = cur_prime_comp_cache->first * cur_sub_comp_cache->first * cur_parameter;
        if (cur_product > max_product) {
          max_product = cur_product;
          max_index = i;
        }
      }
      auto * cache_pair = new std::pair<PsddParameter, uintmax_t>(max_product, max_index);
      cur_node->SetUserData((uintmax_t) cache_pair);
    }
  }
  // Extract solution;
  std::queue<PsddNode*> node_queue;
  node_queue.push(serialized_psdd_nodes[0]);
  std::bitset<MAX_VAR> max_instantiation;
  auto* cache_pair = (std::pair<PsddParameter, uintmax_t>*) serialized_psdd_nodes[0]->user_data();
  auto max_prob = cache_pair->first;
  while (!node_queue.empty()){
    PsddNode* cur_node = node_queue.front();
    node_queue.pop();
    if (cur_node->node_type() == LITERAL_NODE_TYPE){
      PsddLiteralNode * cur_literal_node = cur_node->psdd_literal_node();
      if (cur_literal_node->sign()){
        max_instantiation.set(cur_literal_node->variable_index());
      }
    }else if (cur_node->node_type() == DECISION_NODE_TYPE){
      PsddDecisionNode* cur_decn_node = cur_node->psdd_decision_node();
      cache_pair = (std::pair<PsddParameter, uintmax_t>*) cur_node->user_data();
      node_queue.push(cur_decn_node->primes()[cache_pair->second]);
      node_queue.push(cur_decn_node->subs()[cache_pair->second]);
    }else{
      assert(cur_node->node_type() == TOP_NODE_TYPE);
      PsddTopNode* cur_top_node = cur_node->psdd_top_node();
      cache_pair = (std::pair<PsddParameter, uintmax_t>*) cur_top_node->user_data();
      if (cache_pair->second){
        max_instantiation.set(cur_top_node->variable_index());
      }
    }
  }
  for (PsddNode* cur_node : serialized_psdd_nodes){
    cache_pair = (std::pair<PsddParameter, uintmax_t>*) cur_node->user_data();
    delete(cache_pair);
    cur_node->SetUserData(0);
  }
  return {max_instantiation, max_prob};
}

std::pair<std::bitset<MAX_VAR>, Probability> GetMPESolution(PsddNode *psdd_node) {
  auto serialized_psdd_nodes = psdd_node_util::SerializePsddNodes(psdd_node);
  return GetMPESolution(serialized_psdd_nodes);
}
uintmax_t ModelCount(const std::vector<PsddNode *> &serialized_nodes) {
  std::unordered_map<uintmax_t, uintmax_t> count_cache;
  for (auto node_it = serialized_nodes.rbegin(); node_it != serialized_nodes.rend(); ++node_it) {
    PsddNode *cur_node = *node_it;
    if (cur_node->node_type() == LITERAL_NODE_TYPE) {
      count_cache[cur_node->node_index()] = 1;
    } else if (cur_node->node_type() == TOP_NODE_TYPE) {
      count_cache[cur_node->node_index()] = 2;
    } else {
      assert(cur_node->node_type() == DECISION_NODE_TYPE);
      PsddDecisionNode *cur_decn_node = cur_node->psdd_decision_node();
      const auto &primes = cur_decn_node->primes();
      const auto &subs = cur_decn_node->subs();
      auto element_size = primes.size();
      uintmax_t total_count = 0;
      for (auto i = 0; i < element_size; ++i) {
        PsddNode *cur_prime = primes[i];
        PsddNode *cur_sub = subs[i];
        total_count += count_cache[cur_prime->node_index()] * count_cache[cur_sub->node_index()];
      }
      count_cache[cur_node->node_index()] = total_count;
    }
  }
  return count_cache[serialized_nodes[0]->node_index()];
}
Probability Evaluate(const std::bitset<MAX_VAR> &variables,
                     const std::bitset<MAX_VAR> &instantiation,
                     const std::vector<PsddNode *> &serialized_nodes) {
  std::unordered_map<uintmax_t, Probability> evaluation_cache;
  for (auto node_it = serialized_nodes.rbegin(); node_it != serialized_nodes.rend(); ++node_it) {
    PsddNode *cur_node = *node_it;
    if (cur_node->node_type() == LITERAL_NODE_TYPE) {
      PsddLiteralNode *cur_lit = cur_node->psdd_literal_node();
      if (variables[cur_lit->variable_index()]) {
        if (instantiation[cur_lit->variable_index()] == cur_lit->sign()) {
          evaluation_cache[cur_node->node_index()] = Probability::CreateFromDecimal(1);
        } else {
          evaluation_cache[cur_node->node_index()] = Probability::CreateFromDecimal(0);
        }
      } else {
        evaluation_cache[cur_node->node_index()] = Probability::CreateFromDecimal(1);
      }
    } else if (cur_node->node_type() == TOP_NODE_TYPE) {
      PsddTopNode *cur_top = cur_node->psdd_top_node();
      if (variables[cur_top->variable_index()]) {
        if (instantiation[cur_top->variable_index()]) {
          evaluation_cache[cur_node->node_index()] = cur_top->true_parameter();
        } else {
          evaluation_cache[cur_node->node_index()] = cur_top->false_parameter();
        }
      } else {
        evaluation_cache[cur_node->node_index()] = Probability::CreateFromDecimal(1);
      }
    } else {
      PsddDecisionNode *cur_decn_node = cur_node->psdd_decision_node();
      auto element_size = cur_decn_node->primes().size();
      Probability cur_prob = Probability::CreateFromDecimal(0);
      for (auto i = 0; i < element_size; ++i) {
        PsddNode *cur_prime = cur_decn_node->primes()[i];
        PsddNode *cur_sub = cur_decn_node->subs()[i];
        cur_prob = cur_prob + evaluation_cache[cur_prime->node_index()] * evaluation_cache[cur_sub->node_index()]
            * cur_decn_node->parameters()[i];
      }
      evaluation_cache[cur_node->node_index()] = cur_prob;
    }
  }
  return evaluation_cache[serialized_nodes[0]->node_index()];
}
bool IsConsistent(PsddNode *node,
                  const std::bitset<MAX_VAR> &variable_mask,
                  const std::bitset<MAX_VAR> &partial_instantiation) {
  std::vector<PsddNode *> serialized_nodes = SerializePsddNodes(node);
  return IsConsistent(serialized_nodes, variable_mask, partial_instantiation);
}
bool IsConsistent(const std::vector<PsddNode *> &nodes,
                  const std::bitset<MAX_VAR> &variable_mask,
                  const std::bitset<MAX_VAR> &partial_instantiation) {
  for (auto node_it = nodes.rbegin(); node_it != nodes.rend(); ++node_it) {
    PsddNode *cur_node = *node_it;
    if (cur_node->node_type() == LITERAL_NODE_TYPE) {
      if (variable_mask[cur_node->psdd_literal_node()->variable_index()]) {
        if (partial_instantiation[cur_node->psdd_literal_node()->variable_index()]
            == cur_node->psdd_literal_node()->sign()) {
          cur_node->SetActivationFlag();
        }
      } else {
        cur_node->SetActivationFlag();
      }
    } else if (cur_node->node_type() == TOP_NODE_TYPE) {
      cur_node->SetActivationFlag();
    } else {
      const auto &primes = cur_node->psdd_decision_node()->primes();
      const auto &subs = cur_node->psdd_decision_node()->subs();
      auto element_size = primes.size();
      for (auto i = 0; i < element_size; ++i) {
        if (primes[i]->activation_flag() && subs[i]->activation_flag()) {
          cur_node->SetActivationFlag();
          break;
        }
      }
    }
  }
  bool result = nodes[0]->activation_flag();
  for (PsddNode *cur_node : nodes) {
    cur_node->ResetActivationFlag();
  }
  return result;
}
Probability Evaluate(const std::bitset<MAX_VAR> &variables,
                     const std::bitset<MAX_VAR> &instantiation,
                     PsddNode *root_node) {
  std::vector<PsddNode *> serialized_nodes = SerializePsddNodes(root_node);
  return Evaluate(variables, instantiation, serialized_nodes);
}
}

PsddNode::PsddNode(uintmax_t node_index, Vtree *vtree_node) : PsddNode(node_index, vtree_node, 0) {}

PsddNode::PsddNode(uintmax_t node_index, Vtree *vtree_node, uintmax_t flag_index)
    : node_index_(node_index),
      vtree_node_(vtree_node),
      flag_index_(flag_index),
      activation_flag_(false),
      user_data_(0) {}

uintmax_t PsddNode::node_index() const {
  return node_index_;
}

uintmax_t PsddNode::flag_index() const {
  return flag_index_;
}

std::size_t PsddNode::hash_value() const {
  return hash_value_;
}

void PsddNode::set_hash_value(std::size_t hash_value) {
  hash_value_ = hash_value;
}
Vtree *PsddNode::vtree_node() const {
  return vtree_node_;
}

void PsddNode::CalculateParametersUsingLaplacianSmoothing(PsddParameter alpha) {}

bool PsddNode::activation_flag() const {
  return activation_flag_;
}

void PsddNode::SetActivationFlag() {
  activation_flag_ = true;
}

void PsddNode::ResetActivationFlag() {
  activation_flag_ = false;
}
Probability PsddNode::CalculateLocalProbability() const {
  return Probability::CreateFromDecimal(1);
}
bool PsddNode::IsConsistent(const std::bitset<MAX_VAR> &instantiation, uint32_t variable_size) {
  std::unordered_map<uint32_t, bool> evid;
  for (auto i = 1; i <= variable_size; ++i) {
    evid[i] = instantiation[i];
  }
  return IsConsistent(evid);
}
uintmax_t PsddNode::user_data() const {
  return user_data_;
}
void PsddNode::SetUserData(uintmax_t user_data) {
  user_data_ = user_data;
}

PsddLiteralNode::PsddLiteralNode(uintmax_t node_index, Vtree *vtree_node, uintmax_t flag_index, int32_t literal)
    : PsddNode(node_index, vtree_node, flag_index), literal_(literal) {
  CalculateHashValue();
}

PsddLiteralNode::PsddLiteralNode(uintmax_t *node_index, Vtree *vtree_node, uintmax_t flag_index, int32_t literal)
    : PsddLiteralNode(*node_index, vtree_node, flag_index, literal) {
  *node_index += 1;
}

PsddLiteralNode::PsddLiteralNode(uintmax_t *node_index, Vtree *vtree_node, int32_t literal)
    : PsddLiteralNode(node_index, vtree_node, 0, literal) {}

bool PsddLiteralNode::operator==(const PsddLiteralNode &other) const {
  return literal_ == other.literal() && flag_index() == other.flag_index();
}

int PsddLiteralNode::node_type() const { return 1; }

bool PsddLiteralNode::IsConsistent(const std::unordered_map<uint32_t, bool> &partial_instantiation) const {
  uint32_t var_index = variable_index();
  if (partial_instantiation.find(var_index) != partial_instantiation.end()) {
    return partial_instantiation.find(var_index)->second == sign();
  } else {
    return true;
  }
}

bool PsddLiteralNode::sign() const {
  return literal_ > 0;
}

uint32_t PsddLiteralNode::variable_index() const {
  return literal_ > 0 ? static_cast<uint32_t>(literal_) : static_cast<uint32_t>(-literal_);
}

int32_t PsddLiteralNode::literal() const { return literal_; }

void PsddLiteralNode::CalculateHashValue() {
  std::size_t hash_value = std::hash<int32_t>{}(literal_);
  hash_value ^= (std::hash<uintmax_t>{}(flag_index()) << 1);
  set_hash_value(hash_value);
}

void PsddLiteralNode::ResetDataCount() {}

void PsddLiteralNode::DirectSample(std::bitset<MAX_VAR> *instantiation, RandomDoubleFromUniformGenerator *generator) {
  if (literal_ > 0) {
    instantiation->set((size_t) literal_);
  }
}

PsddDecisionNode::PsddDecisionNode(uintmax_t node_index,
                                   Vtree *vtree_node,
                                   uintmax_t flag_index,
                                   const std::vector<PsddNode *> &primes,
                                   const std::vector<PsddNode *> &subs,
                                   const std::vector<PsddParameter> &parameters) : PsddNode(node_index,
                                                                                            vtree_node,
                                                                                            flag_index),
                                                                                   data_counts_(primes.size(), 0) {
  std::vector<std::pair<uintmax_t, uintmax_t>> indexes;
  for (const auto &cur_prime: primes) {
    indexes.emplace_back(std::make_pair(indexes.size(), cur_prime->node_index()));
  }
  std::sort(indexes.begin(), indexes.end(), [](const auto &lhs, const auto &rhs) {
    return lhs.second < rhs.second;
  });
  auto partition_size = primes.size();
  assert(partition_size == subs.size());
  primes_.resize(partition_size, nullptr);
  subs_.resize(partition_size, nullptr);
  parameters_.resize(partition_size);
  for (auto i = 0; i < partition_size; i++) {
    primes_[i] = primes[indexes[i].first];
    subs_[i] = subs[indexes[i].first];
    if (!parameters.empty()) {
      parameters_[i] = parameters[indexes[i].first];
    }
  }
  CalculateHashValue();
}

PsddDecisionNode::PsddDecisionNode(uintmax_t *node_index,
                                   Vtree *vtree_node,
                                   uintmax_t flag_index,
                                   const std::vector<PsddNode *> &primes,
                                   const std::vector<PsddNode *> &subs,
                                   const std::vector<PsddParameter> &parameters) : PsddDecisionNode(*node_index,
                                                                                                    vtree_node,
                                                                                                    flag_index,
                                                                                                    primes,
                                                                                                    subs,
                                                                                                    parameters) {
  *node_index += 1;
}

PsddDecisionNode::PsddDecisionNode(uintmax_t *node_index,
                                   Vtree *vtree_node,
                                   uintmax_t flag_index,
                                   const std::vector<PsddNode *> &primes,
                                   const std::vector<PsddNode *> &subs) : PsddDecisionNode(node_index,
                                                                                           vtree_node,
                                                                                           flag_index,
                                                                                           primes,
                                                                                           subs,
                                                                                           {}) {
}

PsddDecisionNode::PsddDecisionNode(uintmax_t *node_index,
                                   Vtree *vtree_node,
                                   const std::vector<PsddNode *> &primes,
                                   const std::vector<PsddNode *> &subs) : PsddDecisionNode(node_index,
                                                                                           vtree_node,
                                                                                           0,
                                                                                           primes,
                                                                                           subs) {}

bool PsddDecisionNode::operator==(const PsddDecisionNode &other) const {
  if (primes_.size() != other.primes_.size()) {
    return false;
  }
  if (flag_index() != other.flag_index()) {
    return false;
  }
  auto element_size = primes_.size();
  for (auto i = 0; i < element_size; i++) {
    if (primes_[i]->node_index() != other.primes_[i]->node_index()) {
      return false;
    }
    if (subs_[i]->node_index() != other.subs_[i]->node_index()) {
      return false;
    }
    if (parameters_[i] != other.parameters_[i]) {
      return false;
    }
  }
  return true;
}

int PsddDecisionNode::node_type() const { return 2; }

bool PsddDecisionNode::IsConsistent(const std::unordered_map<uint32_t, bool> &partial_instantiation) const {
  auto element_size = primes_.size();
  for (auto i = 0; i < element_size; i++) {
    PsddNode *cur_prime = primes_[i];
    PsddNode *cur_sub = subs_[i];
    if (cur_prime->IsConsistent(partial_instantiation) && cur_sub->IsConsistent(partial_instantiation)) {
      return true;
    }
  }
  return false;
}

const std::vector<PsddNode *> &PsddDecisionNode::primes() const {
  return primes_;
}

const std::vector<PsddNode *> &PsddDecisionNode::subs() const {
  return subs_;
}

const std::vector<PsddParameter> &PsddDecisionNode::parameters() const {
  return parameters_;
}

void PsddDecisionNode::CalculateHashValue() {
  std::size_t hash_value = std::hash<uintmax_t>{}(flag_index());
  auto element_size = primes_.size();
  for (auto i = 0; i < element_size; i++) {
    hash_value ^= (std::hash<uintmax_t>{}(primes_[i]->node_index()) << i);
    hash_value ^= (std::hash<uintmax_t>{}(subs_[i]->node_index()) << i);
    hash_value ^= (parameters_[i].hash_value() << i);
  }
  set_hash_value(hash_value);
}
void PsddDecisionNode::IncrementDataCount(uintmax_t index, uintmax_t increment_size) {
  data_counts_[index] += increment_size;
}

void PsddDecisionNode::CalculateParametersUsingLaplacianSmoothing(PsddParameter alpha) {
  uintmax_t total_data_count = 0;
  for (const auto cur_data_count : data_counts_) {
    total_data_count += cur_data_count;
  }
  auto element_size = primes_.size();
  double total_data_count_after_smoothing = total_data_count + std::exp(alpha.parameter()) * element_size;
  for (auto i = 0; i < element_size; i++) {
    parameters_[i] = PsddParameter::CreateFromDecimal(
        (data_counts_[i] + std::exp(alpha.parameter())) / total_data_count_after_smoothing);
  }
}
Probability PsddDecisionNode::CalculateLocalProbability() const {
  auto element_size = parameters_.size();
  double cur_result_log = 0;
  for (auto i = 0; i < element_size; ++i) {
    PsddParameter cur_param = parameters_[i];
    uintmax_t cur_data_count = data_counts_[i];
    cur_result_log += cur_data_count * cur_param.parameter();
  }
  return PsddParameter::CreateFromLog(cur_result_log);
}

void PsddDecisionNode::ResetDataCount() {
  auto element_size = primes_.size();
  for (auto i = 0; i < element_size; ++i) {
    data_counts_[i] = 0;
  }
}

void PsddDecisionNode::SampleParameters(RandomDoubleGenerator *generator) {
  auto element_size = primes_.size();
  std::vector<PsddParameter> sampled_number(element_size);
  PsddParameter sum = PsddParameter::CreateFromDecimal(0);
  for (auto i = 0; i < element_size; ++i) {
    double cur_num = generator->generate();
    sampled_number[i] = PsddParameter::CreateFromDecimal(cur_num);
    sum = sum + sampled_number[i];
  }
  for (auto i = 0; i < element_size; ++i) {
    parameters_[i] = sampled_number[i] / sum;
  }
}
void PsddDecisionNode::DirectSample(std::bitset<MAX_VAR> *instantiation, RandomDoubleFromUniformGenerator *generator) {
  PsddParameter uniform_rand = PsddParameter::CreateFromDecimal(
      (generator->generate() - generator->min()) / (generator->max() - generator->min()));
  PsddParameter acc = PsddParameter::CreateFromDecimal(0);
  auto element_size = primes_.size();
  for (auto i = 0; i < element_size; ++i) {
    PsddParameter cur_parameter = parameters_[i];
    acc = acc + cur_parameter;
    if (uniform_rand < acc) {
      // Use this partition
      primes_[i]->DirectSample(instantiation, generator);
      subs_[i]->DirectSample(instantiation, generator);
      return;
    }
  }
  assert(false);
}

PsddTopNode::PsddTopNode(uintmax_t node_index,
                         Vtree *vtree_node,
                         uintmax_t flag_index,
                         uint32_t variable_index,
                         PsddParameter true_parameter,
                         PsddParameter false_parameter) : PsddNode(node_index, vtree_node, flag_index),
                                                          variable_index_(variable_index),
                                                          true_parameter_(true_parameter),
                                                          false_parameter_(false_parameter),
                                                          true_data_count_(0),
                                                          false_data_count_(0) {
  CalculateHashValue();
}

PsddTopNode::PsddTopNode(uintmax_t *node_index, Vtree *vtree_node, uintmax_t flag_index, uint32_t variable_index)
    : PsddTopNode(*node_index, vtree_node, flag_index, variable_index, PsddParameter(), PsddParameter()) {
  *node_index += 1;
}

PsddTopNode::PsddTopNode(uintmax_t *node_index, Vtree *vtree_node, uint32_t variable_index) : PsddTopNode(node_index,
                                                                                                          vtree_node,
                                                                                                          0,
                                                                                                          variable_index) {}

bool PsddTopNode::operator==(const PsddTopNode &other) const {
  return variable_index_ == other.variable_index_ && flag_index() == other.flag_index()
      && true_parameter_ == other.true_parameter_;
}

int PsddTopNode::node_type() const { return 3; }

bool PsddTopNode::IsConsistent(const std::unordered_map<uint32_t, bool> &partial_instantiation) const {
  return true;
}

uint32_t PsddTopNode::variable_index() const { return variable_index_; }

void PsddTopNode::CalculateHashValue() {
  std::size_t hash_value = std::hash<uint32_t>{}(variable_index_);
  hash_value ^= true_parameter_.hash_value() << 1;
  hash_value ^= std::hash<uintmax_t>{}(flag_index()) << 2;
  set_hash_value(hash_value);
}

void PsddTopNode::IncrementTrueDataCount(uintmax_t increment_size) {
  true_data_count_ += increment_size;
}

void PsddTopNode::IncrementFalseDataCount(uintmax_t increment_size) {
  false_data_count_ += increment_size;
}

void PsddTopNode::CalculateParametersUsingLaplacianSmoothing(PsddParameter alpha) {
  double total_data_count =
      true_data_count_ + false_data_count_ + std::exp(alpha.parameter()) + std::exp(alpha.parameter());
  true_parameter_ =
      PsddParameter::CreateFromDecimal((std::exp(alpha.parameter()) + true_data_count_) / total_data_count);
  false_parameter_ =
      PsddParameter::CreateFromDecimal((std::exp(alpha.parameter()) + false_data_count_) / total_data_count);
}

Probability PsddTopNode::CalculateLocalProbability() const {
  double result = true_data_count_ * true_parameter_.parameter() + false_data_count_ * false_parameter_.parameter();
  return Probability::CreateFromLog(result);
}

void PsddTopNode::ResetDataCount() {
  true_data_count_ = 0;
  false_data_count_ = 0;
}
PsddParameter PsddTopNode::true_parameter() const {
  return true_parameter_;
}
PsddParameter PsddTopNode::false_parameter() const {
  return false_parameter_;
}

void PsddTopNode::SampleParameters(RandomDoubleGenerator *generator) {
  double pos_num = generator->generate();
  double neg_num = generator->generate();
  double sum = pos_num + neg_num;
  true_parameter_ = PsddParameter::CreateFromDecimal(pos_num / sum);
  false_parameter_ = PsddParameter::CreateFromDecimal(neg_num / sum);
  double sum_lg = (true_parameter_ + false_parameter_).parameter();
  assert(std::abs(sum_lg) <= 0.0001);
}
void PsddTopNode::DirectSample(std::bitset<MAX_VAR> *instantiation, RandomDoubleFromUniformGenerator *generator) {
  PsddParameter uniform_rand = PsddParameter::CreateFromDecimal(
      (generator->generate() - generator->min()) / (generator->max() - generator->min()));
  if (uniform_rand < true_parameter_) {
    instantiation->set(variable_index_);
  }
}
