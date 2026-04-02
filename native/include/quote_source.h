#pragma once

#include <memory>

#include "models.h"

namespace goldview {

class IQuoteSource {
public:
    virtual ~IQuoteSource() = default;

    virtual QuoteSourceKind kind() const = 0;
    virtual QuoteSourceTransport transport() const = 0;
    virtual bool requiresApiKey() const { return false; }
    virtual PriceSnapshot fetch(const QuoteSourceConfig& config) = 0;
};

std::unique_ptr<IQuoteSource> createQuoteSource(QuoteSourceKind kind);

}  // namespace goldview
