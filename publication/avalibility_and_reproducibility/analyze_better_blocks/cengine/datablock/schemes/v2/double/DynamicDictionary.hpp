#pragma once
#include "datablock/schemes/CScheme.hpp"
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
namespace cengine {
namespace db {
namespace v2 {
namespace d {
// -------------------------------------------------------------------------------------
class DynamicDictionary : public DoubleScheme {
public:
   virtual double expectedCompressionRatio(DoubleStats &stats, u8 allowed_cascading_level);
   u32 compress(const DOUBLE *src, const BITMAP *nullmap, u8 *dest, DoubleStats &stats, u8 allowed_cascading_level) override;
   void decompress(DOUBLE *dest, BitmapWrapper *bitmap, const u8 *src, u32 tuple_count, u32 level) override;
   std::string fullDescription(const u8 *src) override;
   inline virtual DoubleSchemeType schemeType()
   {
      return staticSchemeType();
   }
   inline static DoubleSchemeType staticSchemeType()
   {
      return DoubleSchemeType::X_DICT;
   }
};
// -------------------------------------------------------------------------------------
}
}
}
}
// -------------------------------------------------------------------------------------
