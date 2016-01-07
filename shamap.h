#ifndef SHAMAP_H
#define SHAMAP_H

#include <array>
#include <cassert>
#include <memory>
#include <ostream>
#include <stack>
#include <vector>

using uint256 = std::array<unsigned char, 256/8>;
using SHAMapHash = std::array<unsigned char, 256/8>;
using Blob = std::vector<unsigned char>;

class SHAMapNodeID
{
private:
    uint256 NodeID_ = {};  // Same as SHAMapItem.tag_ in the range [0, depth_)
    unsigned depth_ = 0;

public:
    SHAMapNodeID() = default;
    SHAMapNodeID (void const* ptr, int len);

//     int selectBranch (uint256 const& key) const;
    unsigned depth() const {return depth_;}

    SHAMapNodeID(unsigned depth, uint256 const& hash)
        : NodeID_{hash}
        , depth_{depth}
        {}
};

class SHAMapItem
{
    uint256 tag_;  // prefix same as SHAMapNodeID.NodeID_[0, depth_)
    Blob    data_;
public:
    SHAMapItem(uint256 const& tag, Blob const& data)
        : tag_{tag}
        , data_{data}
        {}

    uint256 const& key() const {return tag_;}

    friend std::ostream& operator<<(std::ostream& os, SHAMapItem const& x);
};

class SHAMapAbstractNode
{
    SHAMapHash hash_;
public:
    virtual ~SHAMapAbstractNode() = 0;
    SHAMapAbstractNode(SHAMapAbstractNode const&) = delete;
    SHAMapAbstractNode& operator=(SHAMapAbstractNode const &) = delete;

    explicit SHAMapAbstractNode(SHAMapHash const& hash)
        : hash_{hash}
        {}

    bool isLeaf () const;

    virtual void display(std::ostream& os, unsigned indent) const = 0;
    virtual void invariants(bool is_root = false) const = 0;
    virtual uint256 const& key() const = 0;
    virtual unsigned depth() const = 0;
    virtual unsigned max_depth(unsigned) const = 0;
};

class SHAMapTreeNode;

class SHAMapInnerNode
    : public SHAMapAbstractNode
{
    SHAMapHash                          hashes_[16] = {{}};
    std::shared_ptr<SHAMapAbstractNode> children_[16];
    unsigned                            isBranch_ = 0;
    unsigned                            depth_ = 0;
    uint256                             common_ = {};
public:
    explicit SHAMapInnerNode(SHAMapHash const& hash)
        : SHAMapAbstractNode{hash}
        {}

    bool isEmptyBranch (int m) const {return (isBranch_ & (1 << m)) == 0;}
    SHAMapAbstractNode* getChildPointer(int m) const {return children_[m].get();}
    std::shared_ptr<SHAMapAbstractNode> firstChild() const;
    std::shared_ptr<SHAMapAbstractNode> getChild(int m) const {return children_[m];}
    void setChild(int branch, std::shared_ptr<SHAMapAbstractNode> const& child);
    void setChildren(std::shared_ptr<SHAMapTreeNode> const& child1,
                     std::shared_ptr<SHAMapTreeNode> const& child2);

    bool has_common_prefix(uint256 const& key) const;
    unsigned get_common_prefix(uint256 const& key) const;
    void set_common(unsigned depth, uint256 const& common);
    uint256 const& common() const {return common_;}
    unsigned numChildren() const;

    void display(std::ostream& os, unsigned indent) const override;
    void invariants(bool is_root = false) const override;
    uint256 const& key() const override;
    unsigned depth() const override;
    unsigned max_depth(unsigned) const override;
};

class SHAMapTreeNode
    : public SHAMapAbstractNode
{
    std::shared_ptr<SHAMapItem const> item_;
public:
    explicit SHAMapTreeNode(SHAMapHash const& hash, SHAMapItem const& item)
        : SHAMapAbstractNode{hash}
        , item_{std::make_shared<SHAMapItem>(item)}
        {}

    std::shared_ptr<SHAMapItem const> const& peekItem () const {return item_;}

    void display(std::ostream& os, unsigned indent) const override;
    void invariants(bool is_root = false) const override;
    uint256 const& key() const override;
    unsigned depth() const override;
    unsigned max_depth(unsigned) const override;
};

inline
bool
SHAMapAbstractNode::isLeaf () const
{
    return dynamic_cast<SHAMapTreeNode const*>(this) != nullptr;
}


class SHAMap
{
    using NodeStack = std::stack<std::pair<SHAMapAbstractNode*, SHAMapNodeID>,
                    std::vector<std::pair<SHAMapAbstractNode*, SHAMapNodeID>>>;

    std::shared_ptr<SHAMapAbstractNode> root_;
public:
    SHAMap();

    bool insert(SHAMapHash const& hash, SHAMapItem const& item);

    friend std::ostream& operator<<(std::ostream& os, SHAMap const& x);

    class const_iterator;
    const_iterator begin() const;
    const_iterator end() const;

    const_iterator findKey(uint256 const& id) const;
    const_iterator upper_bound(uint256 const& id) const;

    const_iterator erase(const_iterator i);

    void display(std::ostream& os) const;

    void invariants() const;
    unsigned max_depth() const;
private:
    SHAMapTreeNode* walkTowardsKey(uint256 const& id, NodeStack* stack = nullptr) const;
    SHAMapItem const* peekFirstItem(NodeStack& stack) const;
    SHAMapItem const* peekNextItem(uint256 const& id, NodeStack& stack) const;
    SHAMapTreeNode* firstBelow(SHAMapAbstractNode*, NodeStack& stack) const;
    SHAMapAbstractNode* descendThrow(SHAMapInnerNode* parent, int branch) const;
    std::shared_ptr<SHAMapAbstractNode>
        descendThrow(std::shared_ptr<SHAMapInnerNode> parent, int branch) const;
};

class SHAMap::const_iterator
{
public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type   = std::ptrdiff_t;
    using value_type        = SHAMapItem;
    using reference         = value_type const&;
    using pointer           = value_type const*;

private:
    NodeStack     stack_;
    SHAMap const* map_  = nullptr;
    pointer       item_ = nullptr;

public:
    const_iterator() = default;

    reference operator*()  const;
    pointer   operator->() const;

    const_iterator& operator++();
    const_iterator  operator++(int);

private:
    explicit const_iterator(SHAMap const* map);
    const_iterator(SHAMap const* map, pointer item);
    const_iterator(SHAMap const* map, pointer item, NodeStack&& stack);

    friend bool operator==(const_iterator const& x, const_iterator const& y);
    friend class SHAMap;
};

inline
SHAMap::const_iterator::const_iterator(SHAMap const* map)
    : map_(map)
    , item_(map_->peekFirstItem(stack_))
{
}

inline
SHAMap::const_iterator::const_iterator(SHAMap const* map, pointer item)
    : map_(map)
    , item_(item)
{
}

inline
SHAMap::const_iterator::const_iterator(SHAMap const* map, pointer item,
                                       NodeStack&& stack)
    : stack_(std::move(stack))
    , map_(map)
    , item_(item)
{
}

inline
SHAMap::const_iterator::reference
SHAMap::const_iterator::operator*() const
{
    return *item_;
}

inline
SHAMap::const_iterator::pointer
SHAMap::const_iterator::operator->() const
{
    return item_;
}

inline
SHAMap::const_iterator&
SHAMap::const_iterator::operator++()
{
    item_ = map_->peekNextItem(item_->key(), stack_);
    return *this;
}

inline
SHAMap::const_iterator
SHAMap::const_iterator::operator++(int)
{
    auto tmp = *this;
    ++(*this);
    return tmp;
}

inline
bool
operator==(SHAMap::const_iterator const& x, SHAMap::const_iterator const& y)
{
    assert(x.map_ == y.map_);
    return x.item_ == y.item_;
}

inline
bool
operator!=(SHAMap::const_iterator const& x, SHAMap::const_iterator const& y)
{
    return !(x == y);
}

inline
SHAMap::const_iterator
SHAMap::begin() const
{
    return const_iterator(this);
}

inline
SHAMap::const_iterator
SHAMap::end() const
{
    return const_iterator(this, nullptr);
}

#endif  // SHAMAP_H
