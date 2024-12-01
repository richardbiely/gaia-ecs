#pragma once
#include "../config/config.h"

#include "../core/iterator.h"

namespace gaia {
	namespace cnt {
		template <class T>
		struct fwd_llist_link {
			//! Pointer the the next element
			T* next = nullptr;
			//! Pointer to the memory address of the previous node's "next". This is not meant for traversal.
			//! It merely allows for maintaining the forward links of the list when removing an item and allows
			//! O(1) removals even in a forward list.
			T** prevs_next = nullptr;

			//! Returns true if the node is linked with another
			GAIA_NODISCARD bool linked() const {
				return next != nullptr || prevs_next != nullptr;
			}
		};

		//! Each fwd_llist node either has to inherit from fwd_llist_base
		//! or it has to provide get_fwd_llist_link() member functions.
		template <class T>
		struct fwd_llist_base {
			fwd_llist_link<T> fwd_link_GAIA;

			fwd_llist_link<T>& get_fwd_llist_link() {
				return fwd_link_GAIA;
			}
			const fwd_llist_link<T>& get_fwd_llist_link() const {
				return fwd_link_GAIA;
			}
		};

		template <typename T>
		struct fwd_llist_iterator {
			using iterator_category = core::forward_iterator_tag;
			using value_type = T;
			using pointer = T*;
			using reference = T&;
			using difference_type = uint32_t;
			using size_type = uint32_t;
			using iterator = fwd_llist_iterator;

		private:
			T* m_pNode;

		public:
			explicit fwd_llist_iterator(T* pNode): m_pNode(pNode) {}

			reference operator*() const {
				return *m_pNode;
			}
			pointer operator->() const {
				return m_pNode;
			}

			iterator& operator++() {
				auto& list = m_pNode->get_fwd_llist_link();
				m_pNode = list.next;
				return *this;
			}
			iterator operator++(int) {
				iterator temp(*this);
				++*this;
				return temp;
			}

			GAIA_NODISCARD bool operator==(const iterator& other) const {
				return m_pNode == other.m_pNode;
			}
			GAIA_NODISCARD bool operator!=(const iterator& other) const {
				return m_pNode != other.m_pNode;
			}
		};

		//! Forward list container.
		//! No memory allocation is performed because the list is stored directly inside
		//! the allocated nodes.
		//! Inserts: O(1)
		//! Removals: O(1)
		//! Iteration: O(N)
		template <class T>
		struct fwd_llist {
			uint32_t count = 0;
			T* first = nullptr;

			//! Clears the list.
			void clear() {
				count = 0;
				first = nullptr;
			}

			//! Links the node in the list.
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

			//! Checks if the node \param pNode is linked in the list.
			GAIA_NODISCARD bool has(T* pNode) const {
				GAIA_ASSERT(pNode != nullptr);

				for (auto& curr: *this) {
					if (&curr == pNode)
						return true;
				}

				return false;
			}

			//! Returns true if the list is empty. False otherwise.
			GAIA_NODISCARD bool empty() const {
				GAIA_ASSERT(count == 0);
				return first == nullptr;
			}

			//! Returns the number of nodes linked in the list.
			GAIA_NODISCARD uint32_t size() const {
				return count;
			}

			fwd_llist_iterator<T> begin() {
				return fwd_llist_iterator<T>(first);
			}

			fwd_llist_iterator<T> end() {
				return fwd_llist_iterator<T>(nullptr);
			}

			fwd_llist_iterator<const T> begin() const {
				return fwd_llist_iterator((const T*)first);
			}

			fwd_llist_iterator<const T> end() const {
				return fwd_llist_iterator((const T*)nullptr);
			}
		};
	} // namespace cnt
} // namespace gaia