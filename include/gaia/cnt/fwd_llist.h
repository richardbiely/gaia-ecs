#pragma once
#include "gaia/config/config.h"

#include "gaia/core/iterator.h"

namespace gaia {
	namespace cnt {
		//! Intrusive links maintained for one forward-list node.
		//! \tparam T Node type.
		template <class T>
		struct fwd_llist_link {
			//! Pointer the the next element
			T* next = nullptr;
			//! Pointer to the memory address of the previous node's "next". This is not meant for traversal.
			//! It merely allows for maintaining the forward links of the list when removing an item, and allows
			//! O(1) removals even in a forward list.
			T** prevs_next = nullptr;

			//! Returns true if the node is linked with another
			//! \return True when this link participates in a list. False otherwise.
			GAIA_NODISCARD bool linked() const {
				return next != nullptr || prevs_next != nullptr;
			}
		};

		//! Each fwd_llist node either has to inherit from fwd_llist_base
		//! or it has to provide get_fwd_llist_link() member functions.
		template <class T>
		struct fwd_llist_base {
			//! Intrusive link storage used by fwd_llist.
			fwd_llist_link<T> fwd_link_GAIA;

			//! Returns the mutable intrusive link.
			//! \return Reference to this node's link storage.
			fwd_llist_link<T>& get_fwd_llist_link() {
				return fwd_link_GAIA;
			}
			//! Returns the immutable intrusive link.
			//! \return Const reference to this node's link storage.
			const fwd_llist_link<T>& get_fwd_llist_link() const {
				return fwd_link_GAIA;
			}
		};

		//! Forward iterator over an intrusive fwd_llist.
		//! \tparam T Node type, optionally const-qualified.
		template <typename T>
		struct fwd_llist_iterator {
			//! Iterator category tag.
			using iterator_category = core::forward_iterator_tag;
			//! Node value type.
			using value_type = T;
			//! Pointer to a node.
			using pointer = T*;
			//! Reference to a node.
			using reference = T&;
			//! Type used for iterator distances.
			using difference_type = uint32_t;
			//! Type used for sizes.
			using size_type = uint32_t;
			//! Iterator type.
			using iterator = fwd_llist_iterator;

		private:
			T* m_pNode;

		public:
			//! Constructs an iterator at a node.
			//! \param pNode Current node, or nullptr for the end sentinel.
			explicit fwd_llist_iterator(T* pNode): m_pNode(pNode) {}

			//! Dereferences the current node.
			//! \return Reference to the current node.
			reference operator*() const {
				return *m_pNode;
			}
			//! Accesses the current node.
			//! \return Pointer to the current node.
			pointer operator->() const {
				return m_pNode;
			}

			//! Advances to the next linked node.
			//! \return This iterator after advancement.
			iterator& operator++() {
				auto& list = m_pNode->get_fwd_llist_link();
				m_pNode = list.next;
				return *this;
			}
			//! Advances to the next linked node.
			//! \return Iterator value before advancement.
			iterator operator++(int) {
				iterator temp(*this);
				++*this;
				return temp;
			}

			//! Compares iterator positions.
			//! \param other Iterator to compare with.
			//! \return True when both iterators refer to the same node.
			GAIA_NODISCARD bool operator==(const iterator& other) const {
				return m_pNode == other.m_pNode;
			}
			//! Compares iterator positions.
			//! \param other Iterator to compare with.
			//! \return True when the iterators refer to different nodes.
			GAIA_NODISCARD bool operator!=(const iterator& other) const {
				return m_pNode != other.m_pNode;
			}
		};

		//! Forward list container.
		//! No memory allocation is performed because the list is stored directly inside allocated nodes.
		//! Inserts: O(1)
		//! Removals: O(1)
		//! Iteration: O(N)
		template <class T>
		struct fwd_llist {
			//! Number of linked nodes.
			uint32_t count = 0;
			//! First linked node, or nullptr when empty.
			T* first = nullptr;

			//! Clears the list.
			void clear() {
				count = 0;
				first = nullptr;
			}

			//! Links the node in the list.
			//! \param pNode Unlinked node to insert at the front.
			void link(T* pNode) {
				GAIA_ASSERT(pNode != nullptr);

				auto& link = pNode->get_fwd_llist_link();
				link.next = first;
				if (first != nullptr) {
					auto& linkFirst = first->get_fwd_llist_link();
					linkFirst.prevs_next = &(link.next);
					first = pNode;
				}
				link.prevs_next = &first;
				first = pNode;

				++count;
			}

			//! Unlinks the node from the list.
			//! \param pNode Node currently linked in this list.
			void unlink(T* pNode) {
				GAIA_ASSERT(pNode != nullptr);

				auto& link = pNode->get_fwd_llist_link();
				*(link.prevs_next) = link.next;
				if (link.next != nullptr) {
					auto& linkNext = link.next->get_fwd_llist_link();
					linkNext.prevs_next = link.prevs_next;
				}

				// Reset the node's link
				link = {};

				--count;
			}

			//! Checks whether a node is linked in this list.
			//! \param pNode Node to find.
			//! \return True when the node belongs to this list. False otherwise.
			GAIA_NODISCARD bool has(T* pNode) const {
				GAIA_ASSERT(pNode != nullptr);

				for (auto& curr: *this) {
					if (&curr == pNode)
						return true;
				}

				return false;
			}

			//! Returns true if the list is empty. False otherwise.
			//! \return True when the list has no nodes. False otherwise.
			GAIA_NODISCARD bool empty() const {
				GAIA_ASSERT(count == 0);
				return first == nullptr;
			}

			//! Returns the number of nodes linked in the list.
			//! \return Number of linked nodes.
			GAIA_NODISCARD uint32_t size() const {
				return count;
			}

			//! Returns an iterator to the first node.
			//! \return Mutable iterator to the first node, or end() when empty.
			fwd_llist_iterator<T> begin() {
				return fwd_llist_iterator<T>(first);
			}

			//! Returns an iterator to the first node.
			//! \return Const iterator to the first node, or end() when empty.
			fwd_llist_iterator<const T> begin() const {
				return fwd_llist_iterator((const T*)first);
			}

			//! Returns an iterator to the first node.
			//! \return Const iterator to the first node, or cend() when empty.
			fwd_llist_iterator<const T> cbegin() const {
				return fwd_llist_iterator((const T*)first);
			}

			//! Returns the mutable end sentinel.
			//! \return Mutable end sentinel.
			fwd_llist_iterator<T> end() {
				return fwd_llist_iterator<T>(nullptr);
			}

			//! Returns the const end sentinel.
			//! \return Const end sentinel.
			fwd_llist_iterator<const T> end() const {
				return fwd_llist_iterator((const T*)nullptr);
			}

			//! Returns the const end sentinel.
			//! \return Const end sentinel.
			fwd_llist_iterator<const T> cend() const {
				return fwd_llist_iterator((const T*)nullptr);
			}
		};
	} // namespace cnt
} // namespace gaia
