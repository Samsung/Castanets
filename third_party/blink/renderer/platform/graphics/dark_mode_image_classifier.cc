// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/dark_mode_image_classifier.h"

#include <map>

#include "base/memory/singleton.h"
#include "base/optional.h"
#include "third_party/blink/renderer/platform/graphics/darkmode/darkmode_classifier.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/skia/include/utils/SkNullCanvas.h"

namespace blink {
namespace {

// Decision tree lower and upper thresholds for grayscale and color images.
const float kLowColorCountThreshold[2] = {0.8125, 0.015137};
const float kHighColorCountThreshold[2] = {1, 0.025635};

bool IsColorGray(const SkColor& color) {
  return abs(static_cast<int>(SkColorGetR(color)) -
             static_cast<int>(SkColorGetG(color))) +
             abs(static_cast<int>(SkColorGetG(color)) -
                 static_cast<int>(SkColorGetB(color))) <=
         8;
}

bool IsColorTransparent(const SkColor& color) {
  return (SkColorGetA(color) < 128);
}

const int kMaxSampledPixels = 1000;
const int kMaxBlocks = 10;
const float kMinOpaquePixelPercentageForForeground = 0.2;

const int kMinImageSizeForClassification1D = 24;
const int kMaxImageSizeForClassification1D = 100;

class DarkModeBitmapImageClassifier : public DarkModeImageClassifier {
  DarkModeClassification DoInitialClassification(const SkRect& dst) override {
    if (dst.width() < kMinImageSizeForClassification1D ||
        dst.height() < kMinImageSizeForClassification1D)
      return DarkModeClassification::kApplyFilter;

    if (dst.width() > kMaxImageSizeForClassification1D ||
        dst.height() > kMaxImageSizeForClassification1D) {
      return DarkModeClassification::kDoNotApplyFilter;
    }

    return DarkModeClassification::kNotClassified;
  }
};

class DarkModeSVGImageClassifier : public DarkModeImageClassifier {
  DarkModeClassification DoInitialClassification(const SkRect& dst) override {
    return DarkModeClassification::kNotClassified;
  }
};

class DarkModeGradientGeneratedImageClassifier
    : public DarkModeImageClassifier {
  DarkModeClassification DoInitialClassification(const SkRect& dst) override {
    return DarkModeClassification::kApplyFilter;
  }
};

// DarkModeImageClassificationCache - Implements classification caches for
// different paint image ids. The classification result for the given |src|
// rect is added to cache identified by |image_id| and result for the same
// can be retrieved. Using Remove(), the cache identified by |image_id| can
// be deleted.
class DarkModeImageClassificationCache {
 public:
  static DarkModeImageClassificationCache* GetInstance() {
    return base::Singleton<DarkModeImageClassificationCache>::get();
  }

  DarkModeClassification Get(PaintImage::Id image_id, const SkRect& src) {
    auto map = cache_.find(image_id);
    if (map == cache_.end())
      return DarkModeClassification::kNotClassified;

    Key key = std::pair<float, float>(src.x(), src.y());
    auto result = map->second.find(key);

    if (result == map->second.end())
      return DarkModeClassification::kNotClassified;

    return result->second;
  }

  void Add(PaintImage::Id image_id,
           const SkRect& src,
           const DarkModeClassification result) {
    DCHECK(Get(image_id, src) == DarkModeClassification::kNotClassified);
    auto map = cache_.find(image_id);
    if (map == cache_.end())
      map = cache_.emplace(image_id, ClassificationMap()).first;

    // TODO(prashant.n): Check weather full |src| should be used or not for
    // key, considering the scenario of same origin and different sizes in the
    // given sprite. Here only location in the image is considered as of now.
    Key key = std::pair<float, float>(src.x(), src.y());
    map->second.emplace(key, result);
  }

  size_t GetSize(PaintImage::Id image_id) {
    auto map = cache_.find(image_id);
    if (map == cache_.end())
      return 0;

    return map->second.size();
  }

  void Remove(PaintImage::Id image_id) { cache_.erase(image_id); }

 private:
  typedef std::pair<float, float> Key;
  typedef std::map<Key, DarkModeClassification> ClassificationMap;

  std::map<PaintImage::Id, ClassificationMap> cache_;

  DarkModeImageClassificationCache() = default;
  ~DarkModeImageClassificationCache() = default;
  friend struct base::DefaultSingletonTraits<DarkModeImageClassificationCache>;

  DISALLOW_COPY_AND_ASSIGN(DarkModeImageClassificationCache);
};

}  // namespace

DarkModeImageClassifier::DarkModeImageClassifier() = default;

DarkModeImageClassifier::~DarkModeImageClassifier() = default;

std::unique_ptr<DarkModeImageClassifier>
DarkModeImageClassifier::MakeBitmapImageClassifier() {
  return std::make_unique<DarkModeBitmapImageClassifier>();
}

std::unique_ptr<DarkModeImageClassifier>
DarkModeImageClassifier::MakeSVGImageClassifier() {
  return std::make_unique<DarkModeSVGImageClassifier>();
}

std::unique_ptr<DarkModeImageClassifier>
DarkModeImageClassifier::MakeGradientGeneratedImageClassifier() {
  return std::make_unique<DarkModeGradientGeneratedImageClassifier>();
}

DarkModeClassification DarkModeImageClassifier::Classify(
    const PaintImage& paint_image,
    const SkRect& src,
    const SkRect& dst) {
  DarkModeImageClassificationCache* cache =
      DarkModeImageClassificationCache::GetInstance();
  PaintImage::Id image_id = paint_image.stable_id();
  DarkModeClassification result = cache->Get(image_id, src);
  if (result != DarkModeClassification::kNotClassified)
    return result;

  result = DoInitialClassification(dst);
  if (result != DarkModeClassification::kNotClassified) {
    cache->Add(image_id, src, result);
    return result;
  }

  auto features_or_null = GetFeatures(paint_image, src);
  if (!features_or_null) {
    // Do not cache this classification.
    return DarkModeClassification::kDoNotApplyFilter;
  }

  result = ClassifyWithFeatures(features_or_null.value());
  cache->Add(image_id, src, result);
  return result;
}

bool DarkModeImageClassifier::GetBitmap(const PaintImage& paint_image,
                                        const SkRect& src,
                                        SkBitmap* bitmap) {
  if (!src.width() || !src.height())
    return false;

  SkRect dst = {0, 0, src.width(), src.height()};

  if (!bitmap || !bitmap->tryAllocPixels(SkImageInfo::MakeN32(
                     static_cast<int>(src.width()),
                     static_cast<int>(src.height()), kPremul_SkAlphaType)))
    return false;

  SkCanvas canvas(*bitmap);
  canvas.clear(SK_ColorTRANSPARENT);
  canvas.drawImageRect(paint_image.GetSkImage(), src, dst, nullptr);
  return true;
}

base::Optional<DarkModeImageClassifier::Features>
DarkModeImageClassifier::GetFeatures(const PaintImage& paint_image,
                                     const SkRect& src) {
  float transparency_ratio;
  float background_ratio;
  std::vector<SkColor> sampled_pixels;
  GetSamples(paint_image, src, &sampled_pixels, &transparency_ratio,
             &background_ratio);
  // TODO(https://crbug.com/945434): Investigate why an incorrect resource is
  // loaded and how we can fetch the correct resource. This condition will
  // prevent going further with the rest of the classification logic.
  if (sampled_pixels.size() == 0)
    return base::nullopt;

  return ComputeFeatures(sampled_pixels, transparency_ratio, background_ratio);
}

// Extracts sample pixels from the image. The image is separated into uniformly
// distributed blocks through its width and height, each block is sampled, and
// checked to see if it seems to be background or foreground.
void DarkModeImageClassifier::GetSamples(const PaintImage& paint_image,
                                         const SkRect& src,
                                         std::vector<SkColor>* sampled_pixels,
                                         float* transparency_ratio,
                                         float* background_ratio) {
  SkBitmap bitmap;
  if (!GetBitmap(paint_image, src, &bitmap))
    return;

  int num_sampled_pixels = kMaxSampledPixels;
  int num_blocks_x = kMaxBlocks;
  int num_blocks_y = kMaxBlocks;

  if (num_sampled_pixels > src.width() * src.height())
    num_sampled_pixels = src.width() * src.height();

  if (num_blocks_x > src.width())
    num_blocks_x = floor(src.width());

  if (num_blocks_y > src.height())
    num_blocks_y = floor(src.height());

  int pixels_per_block = num_sampled_pixels / (num_blocks_x * num_blocks_y);

  int transparent_pixels = 0;
  int opaque_pixels = 0;
  int blocks_count = 0;

  std::vector<int> horizontal_grid(num_blocks_x + 1);
  std::vector<int> vertical_grid(num_blocks_y + 1);

  for (int block = 0; block <= num_blocks_x; block++) {
    horizontal_grid[block] = static_cast<int>(
        round(block * bitmap.width() / static_cast<float>(num_blocks_x)));
  }
  for (int block = 0; block <= num_blocks_y; block++) {
    vertical_grid[block] = static_cast<int>(
        round(block * bitmap.height() / static_cast<float>(num_blocks_y)));
  }

  sampled_pixels->clear();
  std::vector<gfx::Rect> foreground_blocks;

  for (int y = 0; y < num_blocks_y; y++) {
    for (int x = 0; x < num_blocks_x; x++) {
      gfx::Rect block(horizontal_grid[x], vertical_grid[y],
                      horizontal_grid[x + 1] - horizontal_grid[x],
                      vertical_grid[y + 1] - vertical_grid[y]);

      std::vector<SkColor> block_samples;
      int block_transparent_pixels;
      GetBlockSamples(bitmap, block, pixels_per_block, &block_samples,
                      &block_transparent_pixels);
      opaque_pixels += static_cast<int>(block_samples.size());
      transparent_pixels += block_transparent_pixels;
      sampled_pixels->insert(sampled_pixels->end(), block_samples.begin(),
                             block_samples.end());
      if (opaque_pixels >
          kMinOpaquePixelPercentageForForeground * pixels_per_block) {
        foreground_blocks.push_back(block);
      }
      blocks_count++;
    }
  }

  *transparency_ratio = static_cast<float>(transparent_pixels) /
                        (transparent_pixels + opaque_pixels);
  *background_ratio =
      1.0 - static_cast<float>(foreground_blocks.size()) / blocks_count;
}

// Selects samples at regular intervals from a block of the image.
// Returns the opaque sampled pixels, and the number of transparent
// sampled pixels.
void DarkModeImageClassifier::GetBlockSamples(
    const SkBitmap& bitmap,
    const gfx::Rect& block,
    const int required_samples_count,
    std::vector<SkColor>* sampled_pixels,
    int* transparent_pixels_count) {
  *transparent_pixels_count = 0;

  int x1 = block.x();
  int y1 = block.y();
  int x2 = block.right();
  int y2 = block.bottom();
  DCHECK(x1 < bitmap.width());
  DCHECK(y1 < bitmap.height());
  DCHECK(x2 <= bitmap.width());
  DCHECK(y2 <= bitmap.height());

  sampled_pixels->clear();

  int cx = static_cast<int>(
      ceil(static_cast<float>(x2 - x1) / sqrt(required_samples_count)));
  int cy = static_cast<int>(
      ceil(static_cast<float>(y2 - y1) / sqrt(required_samples_count)));

  for (int y = y1; y < y2; y += cy) {
    for (int x = x1; x < x2; x += cx) {
      SkColor new_sample = bitmap.getColor(x, y);
      if (IsColorTransparent(new_sample))
        (*transparent_pixels_count)++;
      else
        sampled_pixels->push_back(new_sample);
    }
  }
}

DarkModeImageClassifier::Features DarkModeImageClassifier::ComputeFeatures(
    const std::vector<SkColor>& sampled_pixels,
    const float transparency_ratio,
    const float background_ratio) {
  int samples_count = static_cast<int>(sampled_pixels.size());

  // Is image grayscale.
  int color_pixels = 0;
  for (const SkColor& sample : sampled_pixels) {
    if (!IsColorGray(sample))
      color_pixels++;
  }
  ColorMode color_mode = (color_pixels > samples_count / 100)
                             ? ColorMode::kColor
                             : ColorMode::kGrayscale;

  DarkModeImageClassifier::Features features;
  features.is_colorful = color_mode == ColorMode::kColor;
  features.color_buckets_ratio =
      ComputeColorBucketsRatio(sampled_pixels, color_mode);
  features.transparency_ratio = transparency_ratio;
  features.background_ratio = background_ratio;

  return features;
}

float DarkModeImageClassifier::ComputeColorBucketsRatio(
    const std::vector<SkColor>& sampled_pixels,
    const ColorMode color_mode) {
  HashSet<unsigned, WTF::AlreadyHashed,
          WTF::UnsignedWithZeroKeyHashTraits<unsigned>>
      buckets;

  // If image is in color, use 4 bits per color channel, otherwise 4 bits for
  // illumination.
  if (color_mode == ColorMode::kColor) {
    for (const SkColor& sample : sampled_pixels) {
      unsigned bucket = ((SkColorGetR(sample) >> 4) << 8) +
                        ((SkColorGetG(sample) >> 4) << 4) +
                        ((SkColorGetB(sample) >> 4));
      buckets.insert(bucket);
    }
  } else {
    for (const SkColor& sample : sampled_pixels) {
      unsigned illumination =
          (SkColorGetR(sample) * 5 + SkColorGetG(sample) * 3 +
           SkColorGetB(sample) * 2) /
          10;
      buckets.insert(illumination / 16);
    }
  }

  // Using 4 bit per channel representation of each color bucket, there would be
  // 2^4 buckets for grayscale images and 2^12 for color images.
  const float max_buckets[] = {16, 4096};
  return static_cast<float>(buckets.size()) /
         max_buckets[color_mode == ColorMode::kColor];
}

DarkModeClassification DarkModeImageClassifier::ClassifyWithFeatures(
    const Features& features) {
  DarkModeClassification result = ClassifyUsingDecisionTree(features);

  // If decision tree cannot decide, we use a neural network to decide whether
  // to filter or not based on all the features.
  if (result == DarkModeClassification::kNotClassified) {
    darkmode_tfnative_model::FixedAllocations nn_temp;
    float nn_out;

    // The neural network expects these features to be in a specific order
    // within float array. Do not change the order here without also changing
    // the neural network code!
    float feature_list[]{features.is_colorful, features.color_buckets_ratio,
                         features.transparency_ratio,
                         features.background_ratio};

    darkmode_tfnative_model::Inference(feature_list, &nn_out, &nn_temp);
    result = nn_out > 0 ? DarkModeClassification::kApplyFilter
                        : DarkModeClassification::kDoNotApplyFilter;
  }

  return result;
}

DarkModeClassification DarkModeImageClassifier::ClassifyUsingDecisionTree(
    const DarkModeImageClassifier::Features& features) {
  float low_color_count_threshold =
      kLowColorCountThreshold[features.is_colorful];
  float high_color_count_threshold =
      kHighColorCountThreshold[features.is_colorful];

  // Very few colors means it's not a photo, apply the filter.
  if (features.color_buckets_ratio < low_color_count_threshold)
    return DarkModeClassification::kApplyFilter;

  // Too many colors means it's probably photorealistic, do not apply it.
  if (features.color_buckets_ratio > high_color_count_threshold)
    return DarkModeClassification::kDoNotApplyFilter;

  // In-between, decision tree cannot give a precise result.
  return DarkModeClassification::kNotClassified;
}

// static
void DarkModeImageClassifier::RemoveCache(PaintImage::Id image_id) {
  DarkModeImageClassificationCache::GetInstance()->Remove(image_id);
}

DarkModeClassification DarkModeImageClassifier::GetCacheValue(
    PaintImage::Id image_id,
    const SkRect& src) {
  return DarkModeImageClassificationCache::GetInstance()->Get(image_id, src);
}

void DarkModeImageClassifier::AddCacheValue(PaintImage::Id image_id,
                                            const SkRect& src,
                                            DarkModeClassification result) {
  return DarkModeImageClassificationCache::GetInstance()->Add(image_id, src,
                                                              result);
}

size_t DarkModeImageClassifier::GetCacheSize(PaintImage::Id image_id) {
  return DarkModeImageClassificationCache::GetInstance()->GetSize(image_id);
}

}  // namespace blink
