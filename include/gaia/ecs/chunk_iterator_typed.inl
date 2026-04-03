#pragma once

namespace gaia {
	namespace ecs {
		namespace detail {
			template <Constraints IterConstraint>
			struct ChunkIterTypedOps {
				struct ArgMeta {
					Entity termId = EntityBad;
					bool isEntity = false;
					bool isPair = false;
				};

				template <typename T>
				static auto arg_meta(const ChunkIterImpl<IterConstraint>& self) -> ArgMeta {
					using Arg = std::remove_cv_t<std::remove_reference_t<T>>;
					if constexpr (std::is_same_v<Arg, Entity>)
						return {.termId = EntityBad, .isEntity = true, .isPair = false};
					else {
						using FT = typename component_type_t<Arg>::TypeFull;
						if constexpr (is_pair<FT>::value)
							return {.termId = EntityBad, .isEntity = false, .isPair = true};
						else
							return {
									.termId = world_query_arg_id<Arg>(*const_cast<World*>(self.world())),
									.isEntity = false,
									.isPair = false};
					}
				}

				template <typename T>
				static auto term_desc(const ChunkIterImpl<IterConstraint>& self) ->
						typename ChunkIterImpl<IterConstraint>::IterTermDesc {
					using Arg = std::remove_cv_t<std::remove_reference_t<T>>;
					const auto meta = arg_meta<T>(self);
					typename ChunkIterImpl<IterConstraint>::IterTermDesc desc;
					desc.termId = meta.termId;
					desc.isEntity = meta.isEntity;
					if constexpr (!mem::is_soa_layout_v<Arg>) {
						if (!meta.isEntity && !meta.isPair)
							desc.isOutOfLine = world_is_out_of_line_component(*self.world(), desc.termId);
					}
					return desc;
				}

				template <typename T>
				static auto view_any(const ChunkIterImpl<IterConstraint>& self) {
					using U = typename actual_type_t<T>::Type;
					if constexpr (std::is_same_v<U, Entity> || mem::is_soa_layout_v<U>)
						return self.m_pChunk->template view<T>(self.from(), self.to());
					else {
						const auto desc = term_desc<T>(self);
						const auto id = desc.isOutOfLine ? desc.termId : EntityBad;

						if (id != EntityBad)
							return EntityTermViewGet<U>::entity(
									self.m_pChunk->entity_view().data() + self.from(), const_cast<World*>(self.world()), id, self.size());

						auto* pData = reinterpret_cast<const U*>(self.m_pChunk->template view<T>(self.from(), self.to()).data());
						return EntityTermViewGet<U>::pointer(pData, self.size());
					}
				}

				template <typename T>
				static auto view_any(ChunkIterImpl<IterConstraint>& self, uint32_t termIdx) {
					using U = typename actual_type_t<T>::Type;

					if constexpr (mem::is_soa_layout_v<U>) {
						const auto compIdx = self.m_pCompIndices[termIdx];
						if (compIdx == 0xFF) {
							const auto* pEntities = self.m_pChunk->entity_view().data() + self.from();
							const auto desc = self.resolved_term_desc(termIdx, term_desc<T>(self));
							GAIA_ASSERT(desc.termId != EntityBad);
							return SoATermViewGet<U>{nullptr,			0, pEntities,	 const_cast<World*>(self.world()),
																			 desc.termId, 0, self.size()};
						}

						GAIA_ASSERT(compIdx < self.m_pChunk->ids_view().size());
						return SoATermViewGet<U>{
								self.m_pChunk->comp_ptr(compIdx),
								self.m_pChunk->capacity(),
								nullptr,
								const_cast<World*>(self.world()),
								EntityBad,
								self.from(),
								self.size()};
					} else {
						const auto desc = self.resolved_term_desc(termIdx, term_desc<T>(self));
						const auto compIdx = self.m_pCompIndices[termIdx];
						if (desc.isOutOfLine)
							return EntityTermViewGet<U>::entity(
									self.m_pChunk->entity_view().data() + self.from(), const_cast<World*>(self.world()), desc.termId,
									self.size());

						if (compIdx == 0xFF) {
							GAIA_ASSERT(desc.termId != EntityBad);
							if (!self.m_inheritedData.empty()) {
								const auto* pInheritedValue = reinterpret_cast<const U*>(self.m_inheritedData[termIdx]);
								if (pInheritedValue != nullptr)
									return EntityTermViewGet<U>::inherited(pInheritedValue, self.size());
							}
							if (world_term_uses_inherit_policy(*self.world(), desc.termId)) {
								return EntityTermViewGet<U>::entity_chunk_stable(
										self.m_pChunk->entity_view().data() + self.from(), self.m_pChunk, const_cast<World*>(self.world()),
										desc.termId, self.from(), self.size());
							}
							return EntityTermViewGet<U>::entity(
									self.m_pChunk->entity_view().data() + self.from(), const_cast<World*>(self.world()), desc.termId,
									self.size());
						}
						GAIA_ASSERT(compIdx < self.m_pChunk->ids_view().size());

						auto* pData = reinterpret_cast<const U*>(self.m_pChunk->comp_ptr_mut(compIdx, self.from()));
						return EntityTermViewGet<U>::pointer(pData, self.size());
					}
				}

				template <typename T>
				static auto view(const ChunkIterImpl<IterConstraint>& self) {
					using U = typename actual_type_t<T>::Type;
					if constexpr (std::is_same_v<U, Entity>)
						return self.m_pChunk->template view<T>(self.from(), self.to());
					else if constexpr (mem::is_soa_layout_v<U>) {
						const auto view = self.m_pChunk->template view<T>(self.from(), self.to());
						return SoATermViewGetPointer<U>{
								(const uint8_t*)view.data(), (uint32_t)view.size(), self.from(), self.size()};
					} else {
						auto* pData = reinterpret_cast<const U*>(self.m_pChunk->template view<T>(self.from(), self.to()).data());
						return EntityTermViewGetPointer<U>{pData, self.size()};
					}
				}

				template <typename T>
				static auto view(const ChunkIterImpl<IterConstraint>& self, uint32_t termIdx) {
					using U = typename actual_type_t<T>::Type;
					const auto compIdx = self.m_pCompIndices[termIdx];
					GAIA_ASSERT(compIdx != 0xFF);

					if constexpr (mem::is_soa_layout_v<U>)
						return SoATermViewGetPointer<U>{
								self.m_pChunk->comp_ptr(compIdx), self.m_pChunk->capacity(), self.from(), self.size()};
					else {
						auto* pData = reinterpret_cast<const U*>(self.m_pChunk->comp_ptr(compIdx, self.from()));
						return EntityTermViewGetPointer<U>{pData, self.size()};
					}
				}

				template <typename T>
				static auto view_any_mut(ChunkIterImpl<IterConstraint>& self) {
					using U = typename actual_type_t<T>::Type;
					static_assert(!std::is_same_v<U, Entity>, "Modifying chunk entities via view_mut is forbidden");
					if constexpr (mem::is_soa_layout_v<U>) {
						const auto desc = term_desc<T>(self);
						self.touch_term_desc(desc);
						if (self.m_writeIm)
							return self.m_pChunk->template view_mut<T>(self.from(), self.to());
						return self.m_pChunk->template sview_mut<T>(self.from(), self.to());
					} else {
						const auto desc = term_desc<T>(self);
						const auto id = desc.isOutOfLine ? desc.termId : EntityBad;

						if (id != EntityBad) {
							self.touch_term(id);
							return self.template entity_view_set<U>(id, self.m_writeIm);
						}

						self.touch_term_desc(desc);
						auto* pData =
								reinterpret_cast<U*>((self.m_writeIm ? self.m_pChunk->template view_mut<T>(self.from(), self.to())
																										 : self.m_pChunk->template sview_mut<T>(self.from(), self.to()))
																				 .data());
						return EntityTermViewSet<U>::pointer(pData, self.size());
					}
				}

				template <typename T>
				static auto view_mut(ChunkIterImpl<IterConstraint>& self) {
					using U = typename actual_type_t<T>::Type;
					static_assert(!std::is_same_v<U, Entity>, "Modifying chunk entities via view_mut is forbidden");
					const auto desc = term_desc<T>(self);
					self.touch_term_desc(desc);
					if constexpr (mem::is_soa_layout_v<U>) {
						auto view = self.m_writeIm ? self.m_pChunk->template view_mut<T>(self.from(), self.to())
																			 : self.m_pChunk->template sview_mut<T>(self.from(), self.to());
						return SoATermViewSetPointer<U>{(uint8_t*)view.data(), (uint32_t)view.size(), self.from(), self.size()};
					} else {
						auto* pData =
								reinterpret_cast<U*>((self.m_writeIm ? self.m_pChunk->template view_mut<T>(self.from(), self.to())
																										 : self.m_pChunk->template sview_mut<T>(self.from(), self.to()))
																				 .data());
						return EntityTermViewSetPointer<U>{pData, self.size()};
					}
				}

				template <typename T>
				static auto view_mut(ChunkIterImpl<IterConstraint>& self, uint32_t termIdx) {
					using U = typename actual_type_t<T>::Type;
					static_assert(!std::is_same_v<U, Entity>, "Modifying chunk entities via view_mut is forbidden");
					const auto compIdx = self.m_pCompIndices[termIdx];
					GAIA_ASSERT(compIdx != 0xFF);
					self.touch_comp_idx(compIdx);

					if constexpr (mem::is_soa_layout_v<U>) {
						if (self.m_writeIm)
							self.m_pChunk->update_world_version(compIdx);
						return SoATermViewSetPointer<U>{
								self.m_pChunk->comp_ptr_mut(compIdx), self.m_pChunk->capacity(), self.from(), self.size()};
					} else {
						if (self.m_writeIm)
							self.m_pChunk->update_world_version(compIdx);
						auto* pData = reinterpret_cast<U*>(self.m_pChunk->comp_ptr_mut(compIdx, self.from()));
						return EntityTermViewSetPointer<U>{pData, self.size()};
					}
				}

				template <typename T>
				static auto view_any_mut(ChunkIterImpl<IterConstraint>& self, uint32_t termIdx) {
					using U = typename actual_type_t<T>::Type;

					if constexpr (mem::is_soa_layout_v<U>) {
						const auto compIdx = self.m_pCompIndices[termIdx];
						if (compIdx == 0xFF) {
							const auto desc = self.resolved_term_desc(termIdx, term_desc<T>(self));
							GAIA_ASSERT(desc.termId != EntityBad);
							self.touch_term(desc.termId);
							return self.template entity_soa_view_set<U>(desc.termId, self.m_writeIm);
						}

						GAIA_ASSERT(compIdx < self.m_pChunk->comp_rec_view().size());
						self.touch_comp_idx(compIdx);
						if (self.m_writeIm)
							self.m_pChunk->update_world_version(compIdx);
						return SoATermViewSet<U>{
								self.m_pChunk->comp_ptr_mut(compIdx),
								self.m_pChunk->capacity(),
								nullptr,
								self.world(),
								EntityBad,
								self.from(),
								self.size(),
								self.m_writeIm};
					} else {
						const auto desc = self.resolved_term_desc(termIdx, term_desc<T>(self));
						const auto compIdx = self.m_pCompIndices[termIdx];
						if (desc.isOutOfLine) {
							self.touch_term(desc.termId);
							return self.template entity_view_set<U>(desc.termId, self.m_writeIm);
						}

						if (compIdx == 0xFF) {
							GAIA_ASSERT(desc.termId != EntityBad);
							self.touch_term(desc.termId);
							return self.template entity_view_set<U>(desc.termId, self.m_writeIm);
						}
						GAIA_ASSERT(compIdx < self.m_pChunk->comp_rec_view().size());

						self.touch_comp_idx(compIdx);
						if (self.m_writeIm)
							self.m_pChunk->update_world_version(compIdx);

						auto* pData = reinterpret_cast<U*>(self.m_pChunk->comp_ptr_mut(compIdx, self.from()));
						return EntityTermViewSet<U>::pointer(pData, self.size());
					}
				}

				template <typename T>
				static auto sview_any_mut(ChunkIterImpl<IterConstraint>& self) {
					using U = typename actual_type_t<T>::Type;
					static_assert(!std::is_same_v<U, Entity>, "Modifying chunk entities via sview_mut is forbidden");
					if constexpr (mem::is_soa_layout_v<U>)
						return self.m_pChunk->template sview_mut<T>(self.from(), self.to());
					else {
						const auto desc = term_desc<T>(self);
						const auto id = desc.isOutOfLine ? desc.termId : EntityBad;

						if (id != EntityBad)
							return self.template entity_view_set<U>(id, false);

						auto* pData = reinterpret_cast<U*>(self.m_pChunk->template sview_mut<T>(self.from(), self.to()).data());
						return EntityTermViewSet<U>::pointer(pData, self.size());
					}
				}

				template <typename T>
				static auto sview_mut(ChunkIterImpl<IterConstraint>& self) {
					using U = typename actual_type_t<T>::Type;
					static_assert(!std::is_same_v<U, Entity>, "Modifying chunk entities via sview_mut is forbidden");
					if constexpr (mem::is_soa_layout_v<U>) {
						auto view = self.m_pChunk->template sview_mut<T>(self.from(), self.to());
						return SoATermViewSetPointer<U>{(uint8_t*)view.data(), (uint32_t)view.size(), self.from(), self.size()};
					} else {
						auto* pData = reinterpret_cast<U*>(self.m_pChunk->template sview_mut<T>(self.from(), self.to()).data());
						return EntityTermViewSetPointer<U>{pData, self.size()};
					}
				}

				template <typename T>
				static auto sview_mut(ChunkIterImpl<IterConstraint>& self, uint32_t termIdx) {
					using U = typename actual_type_t<T>::Type;
					static_assert(!std::is_same_v<U, Entity>, "Modifying chunk entities via sview_mut is forbidden");
					const auto compIdx = self.m_pCompIndices[termIdx];
					GAIA_ASSERT(compIdx != 0xFF);

					if constexpr (mem::is_soa_layout_v<U>)
						return SoATermViewSetPointer<U>{
								self.m_pChunk->comp_ptr_mut(compIdx), self.m_pChunk->capacity(), self.from(), self.size()};
					else {
						auto* pData = reinterpret_cast<U*>(self.m_pChunk->comp_ptr_mut(compIdx, self.from()));
						return EntityTermViewSetPointer<U>{pData, self.size()};
					}
				}

				template <typename T>
				static auto sview_any_mut(ChunkIterImpl<IterConstraint>& self, uint32_t termIdx) {
					using U = typename actual_type_t<T>::Type;

					if constexpr (mem::is_soa_layout_v<U>) {
						const auto compIdx = self.m_pCompIndices[termIdx];
						if (compIdx == 0xFF) {
							const auto desc = self.resolved_term_desc(termIdx, term_desc<T>(self));
							GAIA_ASSERT(desc.termId != EntityBad);
							return self.template entity_soa_view_set<U>(desc.termId, false);
						}

						GAIA_ASSERT(compIdx < self.m_pChunk->ids_view().size());
						return SoATermViewSet<U>{
								self.m_pChunk->comp_ptr_mut(compIdx),
								self.m_pChunk->capacity(),
								nullptr,
								self.world(),
								EntityBad,
								self.from(),
								self.size(),
								false};
					} else {
						const auto desc = self.resolved_term_desc(termIdx, term_desc<T>(self));
						const auto compIdx = self.m_pCompIndices[termIdx];
						if (desc.isOutOfLine)
							return self.template entity_view_set<U>(desc.termId, false);

						if (compIdx == 0xFF) {
							GAIA_ASSERT(desc.termId != EntityBad);
							return self.template entity_view_set<U>(desc.termId, false);
						}
						GAIA_ASSERT(compIdx < self.m_pChunk->ids_view().size());

						auto* pData = reinterpret_cast<U*>(self.m_pChunk->comp_ptr_mut(compIdx, self.from()));
						return EntityTermViewSet<U>::pointer(pData, self.size());
					}
				}

				template <typename T, bool TriggerHooks>
				static void modify(ChunkIterImpl<IterConstraint>& self) {
					self.m_pChunk->template modify<T, TriggerHooks>();
				}

				template <typename T>
				static auto view_auto(ChunkIterImpl<IterConstraint>& self) {
					using UOriginal = typename actual_type_t<T>::TypeOriginal;
					if constexpr (core::is_mut_v<UOriginal>)
						return view_mut<T>(self);
					else
						return view<T>(self);
				}

				template <typename T>
				static auto view_auto_any(ChunkIterImpl<IterConstraint>& self) {
					using UOriginal = typename actual_type_t<T>::TypeOriginal;
					if constexpr (core::is_mut_v<UOriginal>)
						return view_any_mut<T>(self);
					else
						return view_any<T>(self);
				}

				template <typename T>
				static auto sview_auto_any(ChunkIterImpl<IterConstraint>& self) {
					using UOriginal = typename actual_type_t<T>::TypeOriginal;
					if constexpr (core::is_mut_v<UOriginal>)
						return sview_any_mut<T>(self);
					else
						return view_any<T>(self);
				}

				template <typename T>
				static auto sview_auto(ChunkIterImpl<IterConstraint>& self) {
					using UOriginal = typename actual_type_t<T>::TypeOriginal;
					if constexpr (core::is_mut_v<UOriginal>)
						return sview_mut<T>(self);
					else
						return view<T>(self);
				}

				template <typename T>
				static bool has(const ChunkIterImpl<IterConstraint>& self) {
					return self.m_pChunk->template has<T>();
				}

				template <typename T>
				static uint32_t acc_index(const ChunkIterImpl<IterConstraint>& self, uint32_t idx) noexcept {
					using U = typename actual_type_t<T>::Type;
					if constexpr (mem::is_soa_layout_v<U>)
						return idx + self.from();
					else
						return idx;
				}
			};
		} // namespace detail

		template <typename T>
		inline auto CopyIter::view() const {
			return m_pChunk->template view<T>(from(), to());
		}

		template <typename T>
		inline auto CopyIter::view_mut() {
			return m_pChunk->template view_mut<T>(from(), to());
		}

		template <typename T>
		inline auto CopyIter::sview_mut() {
			return m_pChunk->template sview_mut<T>(from(), to());
		}

		template <typename T, bool TriggerHooks>
		inline void CopyIter::modify() {
			m_pChunk->template modify<T, TriggerHooks>();
		}

		template <typename T>
		inline auto CopyIter::view_auto() {
			return m_pChunk->template view_auto<T>(from(), to());
		}

		template <typename T>
		inline auto CopyIter::sview_auto() {
			return m_pChunk->template sview_auto<T>(from(), to());
		}

		template <typename T>
		inline bool CopyIter::has() const {
			return m_pChunk->template has<T>();
		}
	} // namespace ecs
} // namespace gaia
