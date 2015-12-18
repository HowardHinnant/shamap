#include "shamap.h"

SHAMapAbstractNode::~SHAMapAbstractNode() = default;

void
SHAMapInnerNode::setChild(int branch, std::shared_ptr<SHAMapAbstractNode> const& child)
{
    if (child != nullptr)
        isBranch_ |= 1 << branch;
    else
        isBranch_ &= ~(1 << branch);
    children_[branch] = child;
}

// void
// SHAMapInnerNode::setChildCommon(int branch,
//                                 std::shared_ptr<SHAMapAbstractNode> const& child)
// {
//     setChild(branch, child);
//     
// }

void
SHAMapInnerNode::setChildren(std::shared_ptr<SHAMapTreeNode> const& child1,
                             std::shared_ptr<SHAMapTreeNode> const& child2)
{
    auto const& k1 = child1->peekItem()->key();
    auto const& k2 = child2->peekItem()->key();
    assert(k1 != k2);
    for (; k1[depth_] == k2[depth_]; ++depth_)
        common_[depth_] = k1[depth_];
    unsigned b1;
    unsigned b2;
    if ((k1[depth_] & 0xF0) == (k2[depth_] & 0xF0))
    {
        common_[depth_] = k1[depth_] & 0xF0;
        b1 = k1[depth_] & 0x0F;
        b2 = k2[depth_] & 0x0F;
        depth_ = 2*depth_ + 1;
    }
    else
    {
        b1 = k1[depth_] >> 4;
        b2 = k2[depth_] >> 4;
        depth_ = 2*depth_;
    }
    children_[b1] = child1;
    isBranch_ |= 1 << b1;
    children_[b2] = child2;
    isBranch_ |= 1 << b2;
}

bool
SHAMapInnerNode::has_common_prefix(uint256 const& key) const
{
    auto x = common_.begin();
    auto y = key.begin();
    for (unsigned i = 0; i < depth_/2; ++i, ++x, ++y)
    {
        if (*x != *y)
            return false;
    }
    if (depth_ & 1)
    {
        auto i = depth_/2;
        return (*(common_.begin() + i) & 0xF0) == (*(key.begin() + i) & 0xF0);
    }
    return true;
}

unsigned
SHAMapInnerNode::get_common_prefix(uint256 const& key) const
{
    auto x = common_.begin();
    auto y = key.begin();
    auto r = 0;
    for (unsigned i = 0; i < depth_/2; ++i, ++x, ++y, r += 2)
    {
        if (*x != *y)
        {
            if ((*x & 0xF0) == (*y & 0xF0))
                ++r;
            return r;
        }
    }
    if (depth_ & 1)
    {
        auto i = depth_/2;
        if ((*(common_.begin() + i) & 0xF0) == (*(key.begin() + i) & 0xF0))
            ++r;
    }
    return r;
}

void
SHAMapInnerNode::set_common(unsigned depth, uint256 const& common)
{
    depth_ = depth;
    common_ = common;
}

unsigned char
strhex(unsigned char c)
{
    if (c < 10)
        return c + '0';
    return c - 10 + 'A';
}

void
SHAMapInnerNode::display(std::ostream& os, unsigned indent) const
{
    os << std::string(indent, ' ') << "inner{" << depth_ << ", ";
    os << '{';
    for (auto c : common_)
    {
        os << strhex(c >> 4);
        os << strhex(c & 0x0F);
    }
    os << "}, " << std::hex << isBranch_ << std::dec << "}\n";
    for (unsigned branch = 0; branch < 16; ++branch)
    {
        if (children_[branch] == nullptr)
            os << std::string(indent+2, ' ') << "nullptr\n";
        else
            children_[branch]->display(os, indent+2);
    }
}

void
SHAMapInnerNode::invariants(bool is_root) const
{
    unsigned count = 0;
    for (unsigned i = 0; i < 16; ++i)
    {
        if (children_[i] == nullptr)
        {
            assert((isBranch_ & (1 << i)) == 0);
        }
        else
        {
            assert((isBranch_ & (1 << i)) != 0);
            assert(has_common_prefix(children_[i]->key()));
            children_[i]->invariants();
            ++count;
        }
    }
    if (!is_root)
    {
        assert(count >= 2);
        assert(depth_ > 0);
    }
    else
        assert(depth_ == 0);
}

uint256 const&
SHAMapInnerNode::key() const
{
    return common_;
}

unsigned
SHAMapInnerNode::depth() const
{
    return depth_;
}

void
SHAMapTreeNode::display(std::ostream& os, unsigned indent) const
{
    os << std::string(indent, ' ') << "leaf{";
    for (auto c : item_->key())
    {
        os << strhex(c >> 4);
        os << strhex(c & 0x0F);
    }
    os << "}\n";
}

void
SHAMapTreeNode::invariants(bool) const
{
    assert(item_ != nullptr);
}

uint256 const&
SHAMapTreeNode::key() const
{
    return item_->key();
}

unsigned
SHAMapTreeNode::depth() const
{
    return 64;
}

unsigned
SHAMapInnerNode::max_depth(unsigned parent_depth) const
{
    unsigned depth_below_here = 0;
    for (unsigned i = 0; i < 16; ++i)
    {
        if (children_[i] != nullptr)
        {
            depth_below_here = std::max(depth_below_here, children_[i]->max_depth(0));
        }
    }
    return parent_depth + 1 + depth_below_here;
}

unsigned
SHAMapTreeNode::max_depth(unsigned parent_depth) const
{
    return parent_depth + 1;
}

unsigned
SHAMap::max_depth() const
{
    return root_->max_depth(0);
}

std::ostream&
operator<<(std::ostream& os, SHAMapItem const& x)
{
    os << '{';
    for (auto c : x.tag_)
    {
        os << strhex(c >> 4);
        os << strhex(c & 0x0F);
    }
    os << ", ";
    for (auto c : x.data_)
    {
        os << strhex(c >> 4);
        os << strhex(c & 0x0F);
    }
    os << '}';
    return os;
}

std::ostream&
operator<<(std::ostream& os, SHAMap const& x)
{
    os << "{\n";
    for (auto const& i : x)
        os << "    " << i << '\n';
    os << "}";
    return os;
}

void
SHAMap::display(std::ostream& os) const
{
    root_->display(os, 0);
}

SHAMap::SHAMap()
    : root_{std::make_shared<SHAMapInnerNode>(uint256{'r', 'o', 'o', 't'})}
{
}

SHAMapItem const*
SHAMap::peekFirstItem(NodeStack& stack) const
{
    assert(stack.empty());
    SHAMapTreeNode* node = firstBelow(root_.get(), stack);
    if (!node)
    {
        while (!stack.empty())
            stack.pop();
        return nullptr;
    }
    return node->peekItem().get();
}

static
int
selectBranch(unsigned depth, uint256 const& key)
{
    auto branch = *(key.begin() + depth/2);
    if (depth & 1)
        branch &= 0xf;
    else
        branch >>= 4;
    return branch;
}

SHAMapItem const*
SHAMap::peekNextItem(uint256 const& id, NodeStack& stack) const
{
    assert(!stack.empty());
    assert(stack.top().first->isLeaf());
    stack.pop();
    while (!stack.empty())
    {
        auto node = stack.top().first;
        auto nodeID = stack.top().second;
        assert(!node->isLeaf());
        auto inner = static_cast<SHAMapInnerNode*>(node);
        for (auto i = selectBranch(nodeID.depth(), id) + 1; i < 16; ++i)
        {
            if (!inner->isEmptyBranch(i))
            {
                node = descendThrow(inner, i);
                auto leaf = firstBelow(node, stack);
                if (!leaf)
                    throw 3;
                assert(leaf->isLeaf());
                return leaf->peekItem().get();
            }
        }
        stack.pop();
    }
    // must be last item
    return nullptr;
}

SHAMapTreeNode*
SHAMap::firstBelow(SHAMapAbstractNode* node, NodeStack& stack) const
{
    // Return the first item at or below this node
    if (node->isLeaf())
    {
        auto n = static_cast<SHAMapTreeNode*>(node);
        stack.push({n, {64, n->peekItem()->key()}});
        return n;
    }
    auto inner = static_cast<SHAMapInnerNode*>(node);
    stack.push({inner, {inner->depth(), inner->common()}});
    for (int i = 0; i < 16;)
    {
        if (!inner->isEmptyBranch(i))
        {
            node = descendThrow(inner, i);
            assert(!stack.empty());
            if (node->isLeaf())
            {
                auto n = static_cast<SHAMapTreeNode*>(node);
                stack.push({n, {64, n->peekItem()->key()}});
                return n;
            }
            inner = static_cast<SHAMapInnerNode*>(node);
            stack.push({inner, {inner->depth(), inner->common()}});
            i = 0;  // scan all 16 branches of this new node
        }
        else
            ++i;  // scan next branch
    }
    return nullptr;
}

SHAMapAbstractNode*
SHAMap::descendThrow(SHAMapInnerNode* parent, int branch) const
{
    auto ret = parent->getChildPointer(branch);
    if (ret == nullptr && !parent->isEmptyBranch(branch))
        throw 1;
    return ret;
}

std::shared_ptr<SHAMapAbstractNode>
SHAMap::descendThrow(std::shared_ptr<SHAMapInnerNode> parent, int branch) const
{
    auto ret = parent->getChild(branch);
    if (ret == nullptr && !parent->isEmptyBranch(branch))
        throw 2;
    return ret;
}

SHAMapTreeNode*
SHAMap::walkTowardsKey(uint256 const& id, NodeStack* stack) const
{
    assert(stack == nullptr || stack->empty());
    auto inNode = root_.get();
    if (stack != nullptr)
        stack->push({inNode, {inNode->depth(), inNode->key()}});

    while (!inNode->isLeaf())
    {
        auto const branch = selectBranch(inNode->depth(), id);
        auto const inner = static_cast<SHAMapInnerNode*>(inNode);
        if (inner->isEmptyBranch (branch))
            return nullptr;

        inNode = descendThrow (inner, branch);
        if (stack != nullptr)
            stack->push({inNode, {inNode->depth(), inNode->key()}});
    }
    return static_cast<SHAMapTreeNode*>(inNode);
}

SHAMap::const_iterator
SHAMap::findKey(uint256 const& id) const
{
    NodeStack stack;
    SHAMapTreeNode* leaf = walkTowardsKey(id, &stack);
    if (leaf == nullptr || leaf->peekItem()->key() != id)
        return end();
    return const_iterator(this, leaf->peekItem().get(), std::move(stack));
}

SHAMap::const_iterator
SHAMap::upper_bound(uint256 const& id) const
{
    // Get a const_iterator to the next item in the tree after a given item
    // item need not be in tree
    NodeStack stack;
    walkTowardsKey(id, &stack);
    SHAMapAbstractNode* node;
    while (!stack.empty())
    {
        std::tie(node, std::ignore) = stack.top();
        if (node->isLeaf())
        {
            auto leaf = static_cast<SHAMapTreeNode*>(node);
            if (leaf->peekItem()->key() > id)
                return const_iterator(this, leaf->peekItem().get(), std::move(stack));
        }
        else
        {
            auto inner = static_cast<SHAMapInnerNode*>(node);
            for (auto i = selectBranch(inner->depth(), id) + 1; i < 16; ++i)
            {
                if (!inner->isEmptyBranch(i))
                {
                    node = descendThrow(inner, i);
                    auto leaf = firstBelow(node, stack);
                    if (!leaf)
                        throw 4;
                    return const_iterator(this, leaf->peekItem().get(),
                                          std::move(stack));
                }
            }
        }
        stack.pop();
    }
    return end();
}

static
uint256
prefix(unsigned depth, uint256 const& key)
{
    uint256 r{};
    auto x = r.begin();
    auto y = key.begin();
    for (auto i = 0; i < depth/2; ++i, ++x, ++y)
        *x = *y;
    if (depth & 1)
        *x = *y & 0xF0;
    return r;
}

bool
SHAMap::insert(SHAMapHash const& hash, SHAMapItem const& item)
{
    auto key = item.key();
    auto node = root_;
    unsigned depth = 0;
    std::shared_ptr<SHAMapInnerNode> parent;
    int branch;
    while (!node->isLeaf())
    {
        auto inner = std::static_pointer_cast<SHAMapInnerNode>(node);
        if (inner->has_common_prefix(key))
        {
            depth = inner->depth();
            branch = selectBranch(depth, key);
            if (inner->isEmptyBranch(branch))
            {
                // place new leaf here
                inner->setChild(branch, std::make_shared<SHAMapTreeNode>(hash, item));
                return true;
            }
            parent = std::static_pointer_cast<SHAMapInnerNode>(node);
            node = descendThrow(inner, branch);
        }
        else
        {
            // Create new inner node and place old inner node and new leaf below it
            auto parent_depth = depth;
            depth = inner->get_common_prefix(key);
            auto new_inner = std::make_shared<SHAMapInnerNode>(uint256{});
            new_inner->setChild(selectBranch(depth, inner->common()), inner);
            new_inner->setChild(selectBranch(depth, key),
                                std::make_shared<SHAMapTreeNode>(hash, item));
            new_inner->set_common(depth, prefix(depth, key));
            parent->setChild(selectBranch(parent_depth, key), new_inner);
            return true;
        }
    }
    // At leaf.  If this is not a duplicate,
    //   need to create new inner node and insert current leaf and new leaf under it
    auto leaf = std::static_pointer_cast<SHAMapTreeNode>(node);
    if (item.key() != leaf->peekItem()->key())
    {
        auto inner = std::make_shared<SHAMapInnerNode>(uint256{});
        inner->setChildren(leaf, std::make_shared<SHAMapTreeNode>(hash, item));
        parent->setChild(branch, inner);
        return true;
    }
    return false;
}

void
SHAMap::invariants() const
{
    auto node = root_.get();
    assert(node != nullptr);
    node->invariants(true);
}

// int
// SHAMapNodeID::selectBranch(uint256 const& key) const
// {
//     auto branch = *(key.begin() + depth_/2);
//     if (depth_ & 1)
//         branch &= 0xf;
//     else
//         branch >>= 4;
//     return branch;
// }

#include <iostream>
#include <random>
#include <vector>

class sequential
{
    unsigned long long count_ = 0;
public:
    unsigned long long
    operator()()
    {
        return count_++;
    }
};

class sequential256
{
    unsigned long long count_[4] = {0};
    unsigned p_ = 0;
public:
    unsigned long long
    operator()()
    {
        if (p_ == 4)
        {
            count_[0]++;
            p_ = 0;
        }
        return count_[p_++];
    }
};

class sequential256_backwards
{
    unsigned long long count_[4] = {0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF,
                                    0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF};
    unsigned p_ = 4;
public:
    unsigned long long
    operator()()
    {
        if (p_ == 0)
        {
            count_[0]--;
            p_ = 4;
        }
        return count_[--p_];
    }
};

uint256
make_key()
{
    static std::mt19937_64 eng{2};
//     static sequential eng{};
//     static sequential256 eng{};
    uint256 a;
    for (unsigned i = 0; i < 4; ++i)
    {
        auto u = eng();
        for (unsigned j = 0; j < 8; ++j)
        {
            a[i*8+j] = u & 0xFF;
            u >>= 8;
        }
    }
    return a;
}

int
main()
{
    std::vector<uint256> keys;
    for (int i = 0; i < 1000000; ++i)
        keys.push_back(make_key());    
    SHAMap m;
    for (auto const& k : keys)
    {
        m.insert({0}, {k, {}});
//         m.invariants();
    }
//     m.display(std::cout);
//     std::cout << m << '\n';
//     std::cout << m.max_depth() << '\n';
    for (auto const& k : keys)
    {
        auto i = m.findKey(k);
        assert(i != m.end());
        assert(i->key() == k);
        auto j = m.upper_bound(k);
        assert(++i == j);
    }
}
