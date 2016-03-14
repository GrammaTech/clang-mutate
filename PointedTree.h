#ifndef CLANG_MUTATE_STACK_H
#define CLANG_MUTATE_STACK_H

#include <vector>

typedef size_t PTNode;
const PTNode NoNode = 0xffffffff;

template <typename T>
class PointedTree
{
public:

    PointedTree() : m_nodes(), m_point(NoNode) {}
    
    bool isEmpty() const;
    const T & pt() const;
    T & pt();
    void pop();
    void push(const T& x);

    PTNode position() const;
    void moveTo(PTNode node);
    
    std::vector<T> spine() const;
    std::vector<T> spineFrom(PTNode node) const;

    // Helper for automatically restoring a tree's state
    class PointedTreeSnapshot {
    public:
        PointedTreeSnapshot(PointedTree<T> & _tree)
          : tree(_tree), point(_tree.position()) {}
        ~PointedTreeSnapshot()
        { tree.moveTo(point); }
    private:
        PointedTree<T> & tree;
        PTNode point;
    };
    PointedTreeSnapshot snapshot();
    
private:
    struct TreeNode
    {
        TreeNode(const T & _datum, PTNode _parent)
        : datum(_datum), parent(_parent) {}
        T datum;
        PTNode parent;
    };
    
    std::vector<TreeNode> m_nodes;
    PTNode m_point;
};

template <typename T>
typename PointedTree<T>::PointedTreeSnapshot PointedTree<T>::snapshot()
{ return PointedTreeSnapshot(*this); }

template <typename T> bool PointedTree<T>::isEmpty() const
{ return m_nodes.empty() || m_point == NoNode; }

template <typename T> const T & PointedTree<T>::pt() const
{ return m_nodes[m_point].datum; }

template <typename T> T & PointedTree<T>::pt()
{ return m_nodes[m_point].datum; }

template <typename T> PTNode PointedTree<T>::position() const
{ return m_point; }

template <typename T> void PointedTree<T>::moveTo(PTNode node)
{ m_point = node; }
      
template <typename T> void PointedTree<T>::push(const T& x)
{
    m_nodes.push_back(TreeNode(x, m_point));
    m_point = m_nodes.size() - 1;
}

template <typename T> void PointedTree<T>::pop()
{ m_point = m_nodes[m_point].parent; }

template <typename T> std::vector<T> PointedTree<T>::spine() const
{ return spineFrom(m_point); }

template <typename T>
std::vector<T> PointedTree<T>::spineFrom(PTNode node) const
{
    std::vector<T> ans;
    PTNode n = node;
    while (n != NoNode) {
        ans.push_back(m_nodes[n].datum);
        n = m_nodes[n].parent;
    }
    return ans;
}

#endif
