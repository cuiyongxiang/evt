/**
 *  @file
 *  @copyright defined in evt/LICENSE.txt
 */
#include <evt/chain/contracts/group.hpp>
#include <evt/chain/config.hpp>
#include <limits>
#include <fc/crypto/base58.hpp>
#include <fc/crypto/sha256.hpp>
#include <fc/crypto/ripemd160.hpp>
#include <fc/variant.hpp>
#include <evt/chain/exceptions.hpp>

namespace evt { namespace chain { namespace contracts {

void
group::visit_root(const visit_func& func) const {
    EVT_ASSERT(nodes_.size() > 0, group_type_exception, "There's not any node defined in this group");
    return visit_node(nodes_[0], func);
}

void
group::visit_node(const node& node, const visit_func& func) const {
    if(node.is_leaf()) {
        return;
    }
    for(uint i = 0; i < node.size; i++) {
        auto& cnode = nodes_[i + node.index];
        if(!func(cnode)) {
            break;
        }
    }
}

}}}  // namespac evt::chain::contracts

namespace fc {
using namespace evt::chain;
using namespace evt::chain::contracts;

void
to_variant(const group& group, const group::node& node, fc::variant& v) {
    fc::mutable_variant_object mv;
    if(node.is_leaf()) {
        mv["type"] = "leaf";
        to_variant(group.keys_[node.index], mv["key"]);
        mv["weight"] = node.weight;
        v = mv;
        return;
    }
    

    if(node.is_root()) {
        mv["type"] = "root";
    }
    else {
        mv["type"] = "branch";
    }
    mv["threshold"] = node.threshold;
    mv["weight"] = node.weight;

    fc::variants cvs;
    cvs.reserve(node.size);
    for(uint i = 0; i < node.size; i++) {
        auto& cnode = group.nodes_[node.index + i];
        fc::variant cv;
        to_variant(group, cnode, cv);
        cvs.emplace_back(std::move(cv));
    }
    mv["nodes"] = std::move(cvs);
    v = mv;
}

void
to_variant(const evt::chain::contracts::group& group, fc::variant& v) {
    fc::mutable_variant_object mv;
    to_variant(group.id_, mv["id"]);
    to_variant(group.key_, mv["key"]);
    to_variant(group, group.nodes_[0], mv["root"]);
    v = mv;
}

void
from_variant(const fc::variant& v, group& group, group::node& node, int depth) {
    EVT_ASSERT(depth < config::max_recursion_depth, group_type_exception, "Exceeds max node depth");

    auto& vo = v.get_object();
    if(vo.find("threshold") == vo.end()) {
        // leaf node
        node.weight = v["weight"].as<weight_type>();
        node.threshold = 0;
        node.size = 0;
        
        public_key_type key;
        from_variant(v["key"], key);
        EVT_ASSERT(group.keys_.size() < std::numeric_limits<decltype(node.index)>::max(), group_type_exception, "Exceeds max keys limit");
        node.index = group.keys_.size();
        group.keys_.emplace_back(std::move(key));
        return;
    }
    if(depth == 0) {
        // root node
        node.weight = 0;
    }
    else {
        node.weight = v["weight"].as<weight_type>();
    }
    node.threshold = v["threshold"].as<weight_type>();
    
    auto& cvs = v["nodes"].get_array();
    EVT_ASSERT(cvs.size() < std::numeric_limits<decltype(node.size)>::max(), group_type_exception, "Exceeds max child nodes limit");
    EVT_ASSERT(group.nodes_.size() + cvs.size() < std::numeric_limits<decltype(node.index)>::max(), group_type_exception, "Exceeds max nodes limit");
    node.index = group.nodes_.size();
    node.size = cvs.size();

    auto index = node.index;
    group.nodes_.resize(group.nodes_.size() + cvs.size());
    for(uint i = 0; i < cvs.size(); i++) {
        auto cv = cvs[i];
        from_variant(cv, group, group.nodes_[index + i], depth + 1);
    }
}

void
from_variant(const fc::variant& v, evt::chain::contracts::group& group) {
    from_variant(v["key"], group.key_);
    group.id_ = group_id::from_group_key(group.key_);
    group.nodes_.resize(1);
    from_variant(v["root"], group, group.nodes_[0], 0);
}

}  // namespace fc