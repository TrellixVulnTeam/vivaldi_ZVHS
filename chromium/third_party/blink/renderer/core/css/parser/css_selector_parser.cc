
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_selector_parser.h"

#include <memory>
#include "base/auto_reset.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/core/css/css_selector_list.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_observer.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

static void RecordUsageAndDeprecationsOneSelector(
    const CSSSelector* selector,
    const CSSParserContext* context);

namespace {

CSSParserTokenRange ConsumeNestedArgument(CSSParserTokenRange& range) {
  const CSSParserToken& first = range.Peek();
  while (!range.AtEnd() && range.Peek().GetType() != kCommaToken) {
    const CSSParserToken& token = range.Peek();
    if (token.GetBlockType() == CSSParserToken::kBlockStart) {
      range.ConsumeBlock();
      continue;
    }
    range.Consume();
  }
  return range.MakeSubRange(&first, &range.Peek());
}

bool AtEndIgnoringWhitespace(CSSParserTokenRange range) {
  range.ConsumeWhitespace();
  return range.AtEnd();
}

}  // namespace

// static
CSSSelectorVector CSSSelectorParser::ParseSelector(
    CSSParserTokenRange range,
    const CSSParserContext* context,
    StyleSheetContents* style_sheet) {
  CSSSelectorParser parser(context, style_sheet);
  range.ConsumeWhitespace();
  CSSSelectorVector result = parser.ConsumeComplexSelectorList(range);
  if (!range.AtEnd())
    return {};

  parser.RecordUsageAndDeprecations(result);
  return result;
}

// static
CSSSelectorVector CSSSelectorParser::ConsumeSelector(
    CSSParserTokenStream& stream,
    const CSSParserContext* context,
    StyleSheetContents* style_sheet,
    CSSParserObserver* observer) {
  CSSSelectorParser parser(context, style_sheet);
  stream.ConsumeWhitespace();
  CSSSelectorVector result =
      parser.ConsumeComplexSelectorList(stream, observer);
  parser.RecordUsageAndDeprecations(result);
  return result;
}

// static
absl::optional<CSSSelectorList> CSSSelectorParser::ParseScopeBoundary(
    CSSParserTokenRange range,
    const CSSParserContext* context,
    StyleSheetContents* style_sheet) {
  CSSSelectorParser parser(context, style_sheet);
  DisallowPseudoElementsScope disallow_pseudo_elements(&parser);

  range.ConsumeWhitespace();
  CSSSelectorList result = parser.ConsumeForgivingComplexSelectorList(range);
  if (!range.AtEnd())
    return absl::nullopt;
  for (const CSSSelector* current = result.First(); current;
       current = current->TagHistory()) {
    RecordUsageAndDeprecationsOneSelector(current, context);
  }
  return result;
}

// static
bool CSSSelectorParser::SupportsComplexSelector(
    CSSParserTokenRange range,
    const CSSParserContext* context) {
  range.ConsumeWhitespace();
  CSSSelectorParser parser(context, nullptr);
  auto parser_selector = parser.ConsumeComplexSelector(range);
  if (parser.failed_parsing_ || !range.AtEnd() || !parser_selector)
    return false;
  auto complex_selector = parser_selector->ReleaseSelector();
  DCHECK(complex_selector);
  if (ContainsUnknownWebkitPseudoElements(*complex_selector.get()))
    return false;
  return true;
}

CSSSelectorParser::CSSSelectorParser(const CSSParserContext* context,
                                     StyleSheetContents* style_sheet)
    : context_(context), style_sheet_(style_sheet) {}

CSSSelectorVector CSSSelectorParser::ConsumeComplexSelectorList(
    CSSParserTokenRange& range) {
  CSSSelectorVector selector_list;
  std::unique_ptr<CSSParserSelector> selector = ConsumeComplexSelector(range);
  if (!selector)
    return {};
  selector_list.push_back(std::move(selector));
  while (!range.AtEnd() && range.Peek().GetType() == kCommaToken) {
    range.ConsumeIncludingWhitespace();
    selector = ConsumeComplexSelector(range);
    if (!selector)
      return {};
    selector_list.push_back(std::move(selector));
  }

  if (failed_parsing_)
    return {};

  return selector_list;
}

CSSSelectorVector CSSSelectorParser::ConsumeComplexSelectorList(
    CSSParserTokenStream& stream,
    CSSParserObserver* observer) {
  CSSSelectorVector selector_list;

  while (true) {
    const wtf_size_t selector_offset_start = stream.LookAheadOffset();
    CSSParserTokenRange complex_selector =
        stream.ConsumeUntilPeekedTypeIs<kLeftBraceToken, kCommaToken>();
    const wtf_size_t selector_offset_end = stream.LookAheadOffset();

    if (stream.UncheckedAtEnd())
      return {};

    std::unique_ptr<CSSParserSelector> selector =
        ConsumeComplexSelector(complex_selector);
    if (!selector || failed_parsing_ || !complex_selector.AtEnd())
      return {};

    if (observer)
      observer->ObserveSelector(selector_offset_start, selector_offset_end);

    selector_list.push_back(std::move(selector));
    if (stream.Peek().GetType() == kLeftBraceToken)
      break;

    DCHECK_EQ(stream.Peek().GetType(), kCommaToken);
    stream.ConsumeIncludingWhitespace();
  }

  return selector_list;
}

CSSSelectorList CSSSelectorParser::ConsumeCompoundSelectorList(
    CSSParserTokenRange& range) {
  CSSSelectorVector selector_list;
  std::unique_ptr<CSSParserSelector> selector = ConsumeCompoundSelector(range);
  range.ConsumeWhitespace();
  if (!selector)
    return CSSSelectorList();
  selector_list.push_back(std::move(selector));
  while (!range.AtEnd() && range.Peek().GetType() == kCommaToken) {
    range.ConsumeIncludingWhitespace();
    selector = ConsumeCompoundSelector(range);
    range.ConsumeWhitespace();
    if (!selector)
      return CSSSelectorList();
    selector_list.push_back(std::move(selector));
  }

  if (failed_parsing_)
    return CSSSelectorList();

  return CSSSelectorList::AdoptSelectorVector(selector_list);
}

CSSSelectorList CSSSelectorParser::ConsumeNestedSelectorList(
    CSSParserTokenRange& range) {
  if (inside_compound_pseudo_)
    return ConsumeCompoundSelectorList(range);
  CSSSelectorVector result = ConsumeComplexSelectorList(range);
  if (result.IsEmpty())
    return {};
  return CSSSelectorList::AdoptSelectorVector(result);
}

CSSSelectorList CSSSelectorParser::ConsumeForgivingNestedSelectorList(
    CSSParserTokenRange& range) {
  if (inside_compound_pseudo_)
    return ConsumeForgivingCompoundSelectorList(range);
  return ConsumeForgivingComplexSelectorList(range);
}

CSSSelectorList CSSSelectorParser::ConsumeForgivingComplexSelectorList(
    CSSParserTokenRange& range) {
  CSSSelectorVector selector_list;

  while (!range.AtEnd()) {
    base::AutoReset<bool> reset_failure(&failed_parsing_, false);
    CSSParserTokenRange argument = ConsumeNestedArgument(range);
    std::unique_ptr<CSSParserSelector> selector =
        ConsumeComplexSelector(argument);
    if (selector && !failed_parsing_ && argument.AtEnd())
      selector_list.push_back(std::move(selector));
    if (range.Peek().GetType() != kCommaToken)
      break;
    range.ConsumeIncludingWhitespace();
  }

  if (selector_list.IsEmpty())
    return CSSSelectorList();

  return CSSSelectorList::AdoptSelectorVector(selector_list);
}

CSSSelectorList CSSSelectorParser::ConsumeForgivingCompoundSelectorList(
    CSSParserTokenRange& range) {
  CSSSelectorVector selector_list;

  while (!range.AtEnd()) {
    base::AutoReset<bool> reset_failure(&failed_parsing_, false);
    CSSParserTokenRange argument = ConsumeNestedArgument(range);
    std::unique_ptr<CSSParserSelector> selector =
        ConsumeCompoundSelector(argument);
    argument.ConsumeWhitespace();
    if (selector && !failed_parsing_ && argument.AtEnd())
      selector_list.push_back(std::move(selector));
    if (range.Peek().GetType() != kCommaToken)
      break;
    range.ConsumeIncludingWhitespace();
  }

  if (selector_list.IsEmpty())
    return CSSSelectorList();

  return CSSSelectorList::AdoptSelectorVector(selector_list);
}

CSSSelectorList CSSSelectorParser::ConsumeForgivingRelativeSelectorList(
    CSSParserTokenRange& range) {
  CSSSelectorVector selector_list;

  while (!range.AtEnd()) {
    base::AutoReset<bool> reset_failure(&failed_parsing_, false);
    CSSParserTokenRange argument = ConsumeNestedArgument(range);
    std::unique_ptr<CSSParserSelector> selector =
        ConsumeRelativeSelector(argument);
    if (selector && !failed_parsing_ && argument.AtEnd())
      selector_list.push_back(std::move(selector));
    if (range.Peek().GetType() != kCommaToken)
      break;
    range.ConsumeIncludingWhitespace();
  }

  // :has() is not allowed in the pseudos accepting only compound selectors, or
  // not allowed after pseudo elements.
  // (e.g. '::slotted(:has(.a))', '::part(foo):has(:hover)')
  if (inside_compound_pseudo_ ||
      restricting_pseudo_element_ != CSSSelector::kPseudoUnknown) {
    return CSSSelectorList();
  }

  if (selector_list.IsEmpty()) {
    // TODO(blee@igalia.com) Workaround to make :has() unforgiving to avoid
    // JQuery :has() issue: https://github.com/w3c/csswg-drafts/issues/7676
    // Should not fail with empty selector_list
    failed_parsing_ = true;

    return CSSSelectorList();
  }

  return CSSSelectorList::AdoptSelectorVector(selector_list);
}

namespace {

enum CompoundSelectorFlags {
  kHasPseudoElementForRightmostCompound = 1 << 0,
};

unsigned ExtractCompoundFlags(const CSSParserSelector& simple_selector,
                              CSSParserMode parser_mode) {
  if (simple_selector.Match() != CSSSelector::kPseudoElement)
    return 0;
  // We don't restrict what follows custom ::-webkit-* pseudo elements in UA
  // sheets. We currently use selectors in mediaControls.css like this:
  //
  // video::-webkit-media-text-track-region-container.scrolling
  if (parser_mode == kUASheetMode &&
      simple_selector.GetPseudoType() ==
          CSSSelector::kPseudoWebKitCustomElement)
    return 0;
  return kHasPseudoElementForRightmostCompound;
}

}  // namespace

std::unique_ptr<CSSParserSelector> CSSSelectorParser::ConsumeRelativeSelector(
    CSSParserTokenRange& range) {
  std::unique_ptr<CSSParserSelector> selector =
      std::make_unique<CSSParserSelector>();
  selector->SetMatch(CSSSelector::kPseudoClass);
  selector->UpdatePseudoType("-internal-relative-anchor", *context_,
                             false /*has_arguments*/, context_->Mode());
  DCHECK_EQ(selector->GetPseudoType(), CSSSelector::kPseudoRelativeAnchor);

  CSSSelector::RelationType combinator = ConsumeCombinator(range);
  switch (combinator) {
    case CSSSelector::kSubSelector:
    case CSSSelector::kDescendant:
      combinator = CSSSelector::kRelativeDescendant;
      break;
    case CSSSelector::kChild:
      combinator = CSSSelector::kRelativeChild;
      break;
    case CSSSelector::kDirectAdjacent:
      combinator = CSSSelector::kRelativeDirectAdjacent;
      break;
    case CSSSelector::kIndirectAdjacent:
      combinator = CSSSelector::kRelativeIndirectAdjacent;
      break;
    default:
      NOTREACHED();
      return nullptr;
  }

  unsigned previous_compound_flags = 0;

  return ConsumePartialComplexSelector(range, combinator, std::move(selector),
                                       previous_compound_flags);
}

std::unique_ptr<CSSParserSelector> CSSSelectorParser::ConsumeComplexSelector(
    CSSParserTokenRange& range) {
  std::unique_ptr<CSSParserSelector> selector = ConsumeCompoundSelector(range);
  if (!selector)
    return nullptr;

  unsigned previous_compound_flags = 0;

  for (CSSParserSelector* simple = selector.get();
       simple && !previous_compound_flags; simple = simple->TagHistory())
    previous_compound_flags |= ExtractCompoundFlags(*simple, context_->Mode());

  if (CSSSelector::RelationType combinator = ConsumeCombinator(range)) {
    if (is_inside_has_argument_ &&
        is_inside_logical_combination_in_has_argument_) {
      found_complex_logical_combinations_in_has_argument_ = true;
    }
    return ConsumePartialComplexSelector(range, combinator, std::move(selector),
                                         previous_compound_flags);
  }

  return selector;
}

std::unique_ptr<CSSParserSelector>
CSSSelectorParser::ConsumePartialComplexSelector(
    CSSParserTokenRange& range,
    CSSSelector::RelationType& combinator,
    std::unique_ptr<CSSParserSelector> selector,
    unsigned& previous_compound_flags) {
  do {
    std::unique_ptr<CSSParserSelector> next_selector =
        ConsumeCompoundSelector(range);
    if (!next_selector)
      return combinator == CSSSelector::kDescendant ? std::move(selector)
                                                    : nullptr;
    if (previous_compound_flags & kHasPseudoElementForRightmostCompound)
      return nullptr;
    CSSParserSelector* end = next_selector.get();
    unsigned compound_flags = ExtractCompoundFlags(*end, context_->Mode());
    while (end->TagHistory()) {
      end = end->TagHistory();
      compound_flags |= ExtractCompoundFlags(*end, context_->Mode());
    }
    end->SetRelation(combinator);
    previous_compound_flags = compound_flags;
    end->SetTagHistory(std::move(selector));

    selector = std::move(next_selector);
  } while ((combinator = ConsumeCombinator(range)));

  return selector;
}

// static
CSSSelector::PseudoType CSSSelectorParser::ParsePseudoType(
    const AtomicString& name,
    bool has_arguments,
    const Document* document) {
  CSSSelector::PseudoType pseudo_type =
      CSSSelector::NameToPseudoType(name, has_arguments, document);

  if (!RuntimeEnabledFeatures::WebKitScrollbarStylingEnabled()) {
    // Don't convert ::-webkit-scrollbar into webkit custom element pseudos -
    // they should just be treated as unknown pseudos and not have the ability
    // to style shadow/custom elements.
    if (pseudo_type == CSSSelector::kPseudoResizer ||
        pseudo_type == CSSSelector::kPseudoScrollbar ||
        pseudo_type == CSSSelector::kPseudoScrollbarCorner ||
        pseudo_type == CSSSelector::kPseudoScrollbarButton ||
        pseudo_type == CSSSelector::kPseudoScrollbarThumb ||
        pseudo_type == CSSSelector::kPseudoScrollbarTrack ||
        pseudo_type == CSSSelector::kPseudoScrollbarTrackPiece) {
      return CSSSelector::kPseudoUnknown;
    }
  }

  if (pseudo_type != CSSSelector::PseudoType::kPseudoUnknown)
    return pseudo_type;

  if (name.StartsWith("-webkit-"))
    return CSSSelector::PseudoType::kPseudoWebKitCustomElement;
  if (name.StartsWith("-internal-"))
    return CSSSelector::PseudoType::kPseudoBlinkInternalElement;
  if (name.StartsWith("--"))
    return CSSSelector::PseudoType::kPseudoState;

  return CSSSelector::PseudoType::kPseudoUnknown;
}

// static
PseudoId CSSSelectorParser::ParsePseudoElement(const String& selector_string,
                                               const Node* parent) {
  CSSTokenizer tokenizer(selector_string);
  const auto tokens = tokenizer.TokenizeToEOF();
  CSSParserTokenRange range(tokens);

  int number_of_colons = 0;
  while (!range.AtEnd() && range.Peek().GetType() == kColonToken) {
    number_of_colons++;
    range.Consume();
  }

  // TODO(crbug.com/1197620): allowing 0 or 1 preceding colons is not aligned
  // with specs.
  if (!range.AtEnd() && number_of_colons <= 2 &&
      (range.Peek().GetType() == kIdentToken ||
       range.Peek().GetType() == kFunctionToken)) {
    CSSParserToken selector_name_token = range.Consume();
    PseudoId pseudo_id = CSSSelector::GetPseudoId(
        ParsePseudoType(selector_name_token.Value().ToAtomicString(),
                        selector_name_token.GetType() == kFunctionToken,
                        parent ? &parent->GetDocument() : nullptr));

    if (PseudoElement::IsWebExposed(pseudo_id, parent) &&
        ((PseudoElementHasArguments(pseudo_id) &&
          range.Peek(0).GetType() == kIdentToken &&
          range.Peek(1).GetType() == kRightParenthesisToken &&
          range.Peek(2).GetType() == kEOFToken) ||
         range.Peek().GetType() == kEOFToken)) {
      return pseudo_id;
    }
  }

  return kPseudoIdNone;
}

// static
AtomicString CSSSelectorParser::ParsePseudoElementArgument(
    const String& selector_string) {
  CSSTokenizer tokenizer(selector_string);
  const auto tokens = tokenizer.TokenizeToEOF();
  CSSParserTokenRange range(tokens);

  int number_of_colons = 0;
  while (!range.AtEnd() && range.Peek().GetType() == kColonToken) {
    number_of_colons++;
    range.Consume();
  }

  // TODO(crbug.com/1197620): allowing 0 or 1 preceding colons is not aligned
  // with specs.
  if (number_of_colons > 2 || range.Peek(0).GetType() != kFunctionToken ||
      range.Peek(1).GetType() != kIdentToken ||
      range.Peek(2).GetType() != kRightParenthesisToken ||
      range.Peek(3).GetType() != kEOFToken) {
    return g_null_atom;
  }

  return range.Peek(1).Value().ToAtomicString();
}

namespace {

bool IsScrollbarPseudoClass(CSSSelector::PseudoType pseudo) {
  switch (pseudo) {
    case CSSSelector::kPseudoEnabled:
    case CSSSelector::kPseudoDisabled:
    case CSSSelector::kPseudoHover:
    case CSSSelector::kPseudoActive:
    case CSSSelector::kPseudoHorizontal:
    case CSSSelector::kPseudoVertical:
    case CSSSelector::kPseudoDecrement:
    case CSSSelector::kPseudoIncrement:
    case CSSSelector::kPseudoStart:
    case CSSSelector::kPseudoEnd:
    case CSSSelector::kPseudoDoubleButton:
    case CSSSelector::kPseudoSingleButton:
    case CSSSelector::kPseudoNoButton:
    case CSSSelector::kPseudoCornerPresent:
    case CSSSelector::kPseudoWindowInactive:
      return true;
    default:
      return false;
  }
}

bool IsUserActionPseudoClass(CSSSelector::PseudoType pseudo) {
  switch (pseudo) {
    case CSSSelector::kPseudoHover:
    case CSSSelector::kPseudoFocus:
    case CSSSelector::kPseudoFocusVisible:
    case CSSSelector::kPseudoFocusWithin:
    case CSSSelector::kPseudoActive:
      return true;
    default:
      return false;
  }
}

bool IsPseudoClassValidAfterPseudoElement(
    CSSSelector::PseudoType pseudo_class,
    CSSSelector::PseudoType compound_pseudo_element) {
  switch (compound_pseudo_element) {
    case CSSSelector::kPseudoResizer:
    case CSSSelector::kPseudoScrollbar:
    case CSSSelector::kPseudoScrollbarCorner:
    case CSSSelector::kPseudoScrollbarButton:
    case CSSSelector::kPseudoScrollbarThumb:
    case CSSSelector::kPseudoScrollbarTrack:
    case CSSSelector::kPseudoScrollbarTrackPiece:
      return IsScrollbarPseudoClass(pseudo_class);
    case CSSSelector::kPseudoSelection:
      return pseudo_class == CSSSelector::kPseudoWindowInactive;
    case CSSSelector::kPseudoPart:
      return IsUserActionPseudoClass(pseudo_class) ||
             pseudo_class == CSSSelector::kPseudoState;
    case CSSSelector::kPseudoWebKitCustomElement:
    case CSSSelector::kPseudoBlinkInternalElement:
    case CSSSelector::kPseudoFileSelectorButton:
      return IsUserActionPseudoClass(pseudo_class);
    default:
      return false;
  }
}

bool IsSimpleSelectorValidAfterPseudoElement(
    const CSSParserSelector& simple_selector,
    CSSSelector::PseudoType compound_pseudo_element) {
  switch (compound_pseudo_element) {
    case CSSSelector::kPseudoUnknown:
      return true;
    case CSSSelector::kPseudoAfter:
    case CSSSelector::kPseudoBefore:
      if (simple_selector.GetPseudoType() == CSSSelector::kPseudoMarker &&
          RuntimeEnabledFeatures::CSSMarkerNestedPseudoElementEnabled())
        return true;
      break;
    case CSSSelector::kPseudoSlotted:
      return simple_selector.IsTreeAbidingPseudoElement();
    case CSSSelector::kPseudoPart:
      if (simple_selector.IsAllowedAfterPart())
        return true;
      break;
    default:
      break;
  }
  if (simple_selector.Match() != CSSSelector::kPseudoClass)
    return false;
  CSSSelector::PseudoType pseudo = simple_selector.GetPseudoType();
  switch (pseudo) {
    case CSSSelector::kPseudoIs:
    case CSSSelector::kPseudoWhere:
    case CSSSelector::kPseudoNot:
    case CSSSelector::kPseudoHas:
      // These pseudo-classes are themselves always valid.
      // CSSSelectorParser::restricting_pseudo_element_ ensures that invalid
      // nested selectors will be dropped if they are invalid according to
      // this function.
      return true;
    default:
      break;
  }
  return IsPseudoClassValidAfterPseudoElement(pseudo, compound_pseudo_element);
}

bool IsPseudoClassValidWithinHasArgument(CSSParserSelector& selector) {
  DCHECK_EQ(selector.Match(), CSSSelector::kPseudoClass);
  switch (selector.GetPseudoType()) {
    // Limited nested :has() to avoid increasing :has() invalidation complexity.
    case CSSSelector::kPseudoHas:
      return false;
    default:
      return true;
  }
}

}  // namespace

std::unique_ptr<CSSParserSelector> CSSSelectorParser::ConsumeCompoundSelector(
    CSSParserTokenRange& range) {
  base::AutoReset<CSSSelector::PseudoType> reset_restricting(
      &restricting_pseudo_element_, restricting_pseudo_element_);

  std::unique_ptr<CSSParserSelector> compound_selector;
  AtomicString namespace_prefix;
  AtomicString element_name;
  const bool has_q_name = ConsumeName(range, element_name, namespace_prefix);
  if (!has_q_name) {
    compound_selector = ConsumeSimpleSelector(range);
    if (!compound_selector)
      return nullptr;
    if (compound_selector->Match() == CSSSelector::kPseudoElement)
      restricting_pseudo_element_ = compound_selector->GetPseudoType();
  }
  if (context_->IsHTMLDocument())
    element_name = element_name.LowerASCII();

  while (std::unique_ptr<CSSParserSelector> simple_selector =
             ConsumeSimpleSelector(range)) {
    if (simple_selector->Match() == CSSSelector::kPseudoElement)
      restricting_pseudo_element_ = simple_selector->GetPseudoType();

    if (compound_selector)
      compound_selector = AddSimpleSelectorToCompound(
          std::move(compound_selector), std::move(simple_selector));
    else
      compound_selector = std::move(simple_selector);
  }

  // While inside a nested selector like :is(), the default namespace shall
  // be ignored when [1]:
  //
  // - The compound selector represents the subject [2], and
  // - The compound selector does not contain a type/universal selector.
  //
  // [1] https://drafts.csswg.org/selectors/#matches
  // [2] https://drafts.csswg.org/selectors/#selector-subject
  base::AutoReset<bool> ignore_namespace(
      &ignore_default_namespace_,
      ignore_default_namespace_ || (resist_default_namespace_ && !has_q_name &&
                                    AtEndIgnoringWhitespace(range)));

  if (!compound_selector) {
    AtomicString namespace_uri = DetermineNamespace(namespace_prefix);
    if (namespace_uri.IsNull()) {
      context_->Count(WebFeature::kCSSUnknownNamespacePrefixInSelector);
      failed_parsing_ = true;
      return nullptr;
    }
    if (namespace_uri == DefaultNamespace())
      namespace_prefix = g_null_atom;
    context_->Count(WebFeature::kHasIDClassTagAttribute);
    return std::make_unique<CSSParserSelector>(
        QualifiedName(namespace_prefix, element_name, namespace_uri));
  }
  // TODO(futhark@chromium.org): Prepending a type selector to the compound is
  // unnecessary if this compound is an argument to a pseudo selector like
  // :not(), since a type selector will be prepended at the top level of the
  // selector if necessary. We need to propagate that context information here
  // to tell if we are at the top level.
  PrependTypeSelectorIfNeeded(namespace_prefix, has_q_name, element_name,
                              compound_selector.get());
  return SplitCompoundAtImplicitShadowCrossingCombinator(
      std::move(compound_selector));
}

std::unique_ptr<CSSParserSelector> CSSSelectorParser::ConsumeSimpleSelector(
    CSSParserTokenRange& range) {
  const CSSParserToken& token = range.Peek();
  std::unique_ptr<CSSParserSelector> selector;
  if (token.GetType() == kHashToken)
    selector = ConsumeId(range);
  else if (token.GetType() == kDelimiterToken && token.Delimiter() == '.')
    selector = ConsumeClass(range);
  else if (token.GetType() == kLeftBracketToken)
    selector = ConsumeAttribute(range);
  else if (token.GetType() == kColonToken)
    selector = ConsumePseudo(range);
  else
    return nullptr;
  // TODO(futhark@chromium.org): crbug.com/578131
  // The UASheetMode check is a work-around to allow this selector in
  // mediaControls(New).css:
  // video::-webkit-media-text-track-region-container.scrolling
  if (!selector || (context_->Mode() != kUASheetMode &&
                    !IsSimpleSelectorValidAfterPseudoElement(
                        *selector.get(), restricting_pseudo_element_))) {
    failed_parsing_ = true;
  }
  return selector;
}

bool CSSSelectorParser::ConsumeName(CSSParserTokenRange& range,
                                    AtomicString& name,
                                    AtomicString& namespace_prefix) {
  name = g_null_atom;
  namespace_prefix = g_null_atom;

  const CSSParserToken& first_token = range.Peek();
  if (first_token.GetType() == kIdentToken) {
    name = first_token.Value().ToAtomicString();
    range.Consume();
  } else if (first_token.GetType() == kDelimiterToken &&
             first_token.Delimiter() == '*') {
    name = CSSSelector::UniversalSelectorAtom();
    range.Consume();
  } else if (first_token.GetType() == kDelimiterToken &&
             first_token.Delimiter() == '|') {
    // This is an empty namespace, which'll get assigned this value below
    name = g_empty_atom;
  } else {
    return false;
  }

  if (range.Peek().GetType() != kDelimiterToken ||
      range.Peek().Delimiter() != '|')
    return true;

  namespace_prefix =
      name == CSSSelector::UniversalSelectorAtom() ? g_star_atom : name;
  if (range.Peek(1).GetType() == kIdentToken) {
    range.Consume();
    name = range.Consume().Value().ToAtomicString();
  } else if (range.Peek(1).GetType() == kDelimiterToken &&
             range.Peek(1).Delimiter() == '*') {
    range.Consume();
    range.Consume();
    name = CSSSelector::UniversalSelectorAtom();
  } else {
    name = g_null_atom;
    namespace_prefix = g_null_atom;
    return false;
  }

  return true;
}

std::unique_ptr<CSSParserSelector> CSSSelectorParser::ConsumeId(
    CSSParserTokenRange& range) {
  DCHECK_EQ(range.Peek().GetType(), kHashToken);
  if (range.Peek().GetHashTokenType() != kHashTokenId)
    return nullptr;
  std::unique_ptr<CSSParserSelector> selector =
      std::make_unique<CSSParserSelector>();
  selector->SetMatch(CSSSelector::kId);
  AtomicString value = range.Consume().Value().ToAtomicString();
  selector->SetValue(value, IsQuirksModeBehavior(context_->Mode()));
  context_->Count(WebFeature::kHasIDClassTagAttribute);
  return selector;
}

std::unique_ptr<CSSParserSelector> CSSSelectorParser::ConsumeClass(
    CSSParserTokenRange& range) {
  DCHECK_EQ(range.Peek().GetType(), kDelimiterToken);
  DCHECK_EQ(range.Peek().Delimiter(), '.');
  range.Consume();
  if (range.Peek().GetType() != kIdentToken)
    return nullptr;
  std::unique_ptr<CSSParserSelector> selector =
      std::make_unique<CSSParserSelector>();
  selector->SetMatch(CSSSelector::kClass);
  AtomicString value = range.Consume().Value().ToAtomicString();
  selector->SetValue(value, IsQuirksModeBehavior(context_->Mode()));
  context_->Count(WebFeature::kHasIDClassTagAttribute);
  return selector;
}

std::unique_ptr<CSSParserSelector> CSSSelectorParser::ConsumeAttribute(
    CSSParserTokenRange& range) {
  DCHECK_EQ(range.Peek().GetType(), kLeftBracketToken);
  CSSParserTokenRange block = range.ConsumeBlock();
  block.ConsumeWhitespace();

  AtomicString namespace_prefix;
  AtomicString attribute_name;
  if (!ConsumeName(block, attribute_name, namespace_prefix))
    return nullptr;
  if (attribute_name == CSSSelector::UniversalSelectorAtom())
    return nullptr;
  block.ConsumeWhitespace();

  if (context_->IsHTMLDocument())
    attribute_name = attribute_name.LowerASCII();

  AtomicString namespace_uri = DetermineNamespace(namespace_prefix);
  if (namespace_uri.IsNull())
    return nullptr;

  QualifiedName qualified_name =
      namespace_prefix.IsNull()
          ? QualifiedName(g_null_atom, attribute_name, g_null_atom)
          : QualifiedName(namespace_prefix, attribute_name, namespace_uri);

  std::unique_ptr<CSSParserSelector> selector =
      std::make_unique<CSSParserSelector>();

  if (block.AtEnd()) {
    selector->SetAttribute(qualified_name,
                           CSSSelector::AttributeMatchType::kCaseSensitive);
    selector->SetMatch(CSSSelector::kAttributeSet);
    context_->Count(WebFeature::kHasIDClassTagAttribute);
    return selector;
  }

  selector->SetMatch(ConsumeAttributeMatch(block));

  const CSSParserToken& attribute_value = block.ConsumeIncludingWhitespace();
  if (attribute_value.GetType() != kIdentToken &&
      attribute_value.GetType() != kStringToken)
    return nullptr;
  selector->SetValue(attribute_value.Value().ToAtomicString());
  selector->SetAttribute(qualified_name, ConsumeAttributeFlags(block));

  if (!block.AtEnd())
    return nullptr;
  context_->Count(WebFeature::kHasIDClassTagAttribute);
  return selector;
}

std::unique_ptr<CSSParserSelector> CSSSelectorParser::ConsumePseudo(
    CSSParserTokenRange& range) {
  DCHECK_EQ(range.Peek().GetType(), kColonToken);
  range.Consume();

  int colons = 1;
  if (range.Peek().GetType() == kColonToken) {
    range.Consume();
    colons++;
  }

  const CSSParserToken& token = range.Peek();
  if (token.GetType() != kIdentToken && token.GetType() != kFunctionToken)
    return nullptr;

  std::unique_ptr<CSSParserSelector> selector =
      std::make_unique<CSSParserSelector>();
  selector->SetMatch(colons == 1 ? CSSSelector::kPseudoClass
                                 : CSSSelector::kPseudoElement);

  AtomicString value = token.Value().ToAtomicString();
  bool has_arguments = token.GetType() == kFunctionToken;
  selector->UpdatePseudoType(value, *context_, has_arguments, context_->Mode());

  if (selector->Match() == CSSSelector::kPseudoElement) {
    switch (selector->GetPseudoType()) {
      case CSSSelector::kPseudoBefore:
      case CSSSelector::kPseudoAfter:
        context_->Count(WebFeature::kHasBeforeOrAfterPseudoElement);
        break;
      case CSSSelector::kPseudoMarker:
        if (context_->Mode() != kUASheetMode)
          context_->Count(WebFeature::kHasMarkerPseudoElement);
        break;
      default:
        break;
    }
  }

  if (selector->Match() == CSSSelector::kPseudoElement &&
      disallow_pseudo_elements_)
    return nullptr;

  if (is_inside_has_argument_) {
    DCHECK(disallow_pseudo_elements_);
    if (!IsPseudoClassValidWithinHasArgument(*selector))
      return nullptr;
    found_pseudo_in_has_argument_ = true;
  }

  if (token.GetType() == kIdentToken) {
    range.Consume();
    if (selector->GetPseudoType() == CSSSelector::kPseudoUnknown)
      return nullptr;
    return selector;
  }

  CSSParserTokenRange block = range.ConsumeBlock();
  block.ConsumeWhitespace();
  if (selector->GetPseudoType() == CSSSelector::kPseudoUnknown)
    return nullptr;

  switch (selector->GetPseudoType()) {
    case CSSSelector::kPseudoIs: {
      DisallowPseudoElementsScope scope(this);
      base::AutoReset<bool> resist_namespace(&resist_default_namespace_, true);
      base::AutoReset<bool> is_inside_logical_combination_in_has_argument(
          &is_inside_logical_combination_in_has_argument_,
          is_inside_has_argument_);

      std::unique_ptr<CSSSelectorList> selector_list =
          std::make_unique<CSSSelectorList>();
      *selector_list = ConsumeForgivingNestedSelectorList(block);
      if (!block.AtEnd())
        return nullptr;
      selector->SetSelectorList(std::move(selector_list));
      return selector;
    }
    case CSSSelector::kPseudoWhere: {
      DisallowPseudoElementsScope scope(this);
      base::AutoReset<bool> resist_namespace(&resist_default_namespace_, true);
      base::AutoReset<bool> is_inside_logical_combination_in_has_argument(
          &is_inside_logical_combination_in_has_argument_,
          is_inside_has_argument_);

      std::unique_ptr<CSSSelectorList> selector_list =
          std::make_unique<CSSSelectorList>();
      *selector_list = ConsumeForgivingNestedSelectorList(block);
      if (!block.AtEnd())
        return nullptr;
      selector->SetSelectorList(std::move(selector_list));
      return selector;
    }
    case CSSSelector::kPseudoHost:
    case CSSSelector::kPseudoHostContext:
    case CSSSelector::kPseudoAny:
    case CSSSelector::kPseudoCue: {
      DisallowPseudoElementsScope scope(this);
      base::AutoReset<bool> inside_compound(&inside_compound_pseudo_, true);
      base::AutoReset<bool> ignore_namespace(
          &ignore_default_namespace_,
          ignore_default_namespace_ ||
              selector->GetPseudoType() == CSSSelector::kPseudoCue);

      std::unique_ptr<CSSSelectorList> selector_list =
          std::make_unique<CSSSelectorList>();
      *selector_list = ConsumeCompoundSelectorList(block);
      if (!selector_list->IsValid() || !block.AtEnd())
        return nullptr;

      if (!selector_list->HasOneSelector()) {
        if (selector->GetPseudoType() == CSSSelector::kPseudoHost)
          return nullptr;
        if (selector->GetPseudoType() == CSSSelector::kPseudoHostContext)
          return nullptr;
      }

      selector->SetSelectorList(std::move(selector_list));
      return selector;
    }
    case CSSSelector::kPseudoHas: {
      if (!RuntimeEnabledFeatures::CSSPseudoHasEnabled())
        return nullptr;

      DisallowPseudoElementsScope scope(this);
      base::AutoReset<bool> resist_namespace(&resist_default_namespace_, true);

      base::AutoReset<bool> is_inside_has_argument(&is_inside_has_argument_,
                                                   true);
      base::AutoReset<bool> found_pseudo_in_has_argument(
          &found_pseudo_in_has_argument_, false);
      base::AutoReset<bool> found_complex_logical_combinations_in_has_argument(
          &found_complex_logical_combinations_in_has_argument_, false);

      std::unique_ptr<CSSSelectorList> selector_list =
          std::make_unique<CSSSelectorList>();
      *selector_list = ConsumeForgivingRelativeSelectorList(block);

      // TODO(blee@igalia.com) Workaround to make :has() unforgiving to avoid
      // JQuery :has() issue: https://github.com/w3c/csswg-drafts/issues/7676
      // Should not check IsValid().
      if (!selector_list->IsValid())
        return nullptr;

      if (!block.AtEnd())
        return nullptr;

      selector->SetSelectorList(std::move(selector_list));
      if (found_pseudo_in_has_argument_)
        selector->SetContainsPseudoInsideHasPseudoClass();
      if (found_complex_logical_combinations_in_has_argument_)
        selector->SetContainsComplexLogicalCombinationsInsideHasPseudoClass();
      return selector;
    }
    case CSSSelector::kPseudoNot: {
      DisallowPseudoElementsScope scope(this);
      base::AutoReset<bool> resist_namespace(&resist_default_namespace_, true);
      base::AutoReset<bool> is_inside_logical_combination_in_has_argument(
          &is_inside_logical_combination_in_has_argument_,
          is_inside_has_argument_);

      std::unique_ptr<CSSSelectorList> selector_list =
          std::make_unique<CSSSelectorList>();
      *selector_list = ConsumeNestedSelectorList(block);
      if (!selector_list->IsValid() || !block.AtEnd())
        return nullptr;

      selector->SetSelectorList(std::move(selector_list));
      return selector;
    }
    case CSSSelector::kPseudoDir: {
      const CSSParserToken& ident = block.ConsumeIncludingWhitespace();
      if (ident.GetType() != kIdentToken || !block.AtEnd())
        return nullptr;
      selector->SetArgument(ident.Value().ToAtomicString());
      return selector;
    }
    case CSSSelector::kPseudoPart: {
      Vector<AtomicString> parts;
      do {
        const CSSParserToken& ident = block.ConsumeIncludingWhitespace();
        if (ident.GetType() != kIdentToken)
          return nullptr;
        parts.push_back(ident.Value().ToAtomicString());
      } while (!block.AtEnd());
      selector->SetPartNames(std::make_unique<Vector<AtomicString>>(parts));
      return selector;
    }
    case CSSSelector::kPseudoPageTransitionContainer:
    case CSSSelector::kPseudoPageTransitionImageWrapper:
    case CSSSelector::kPseudoPageTransitionOutgoingImage:
    case CSSSelector::kPseudoPageTransitionIncomingImage: {
      const CSSParserToken& ident = block.ConsumeIncludingWhitespace();
      if (!block.AtEnd())
        return nullptr;

      absl::optional<AtomicString> argument;
      if (ident.GetType() == kIdentToken)
        argument = ident.Value().ToAtomicString();
      else if (ident.GetType() == kDelimiterToken && ident.Delimiter() == '*')
        argument = CSSSelector::UniversalSelectorAtom();

      if (!argument)
        return nullptr;

      selector->SetArgument(*argument);
      return selector;
    }
    case CSSSelector::kPseudoSlotted: {
      DisallowPseudoElementsScope scope(this);
      base::AutoReset<bool> inside_compound(&inside_compound_pseudo_, true);

      std::unique_ptr<CSSParserSelector> inner_selector =
          ConsumeCompoundSelector(block);
      block.ConsumeWhitespace();
      if (!inner_selector || !block.AtEnd())
        return nullptr;
      CSSSelectorVector selector_vector;
      selector_vector.push_back(std::move(inner_selector));
      selector->AdoptSelectorVector(selector_vector);
      return selector;
    }
    case CSSSelector::kPseudoLang: {
      // FIXME: CSS Selectors Level 4 allows :lang(*-foo)
      const CSSParserToken& ident = block.ConsumeIncludingWhitespace();
      if (ident.GetType() != kIdentToken || !block.AtEnd())
        return nullptr;
      selector->SetArgument(ident.Value().ToAtomicString());
      return selector;
    }
    case CSSSelector::kPseudoNthChild:
    case CSSSelector::kPseudoNthLastChild:
    case CSSSelector::kPseudoNthOfType:
    case CSSSelector::kPseudoNthLastOfType: {
      std::pair<int, int> ab;
      if (!ConsumeANPlusB(block, ab))
        return nullptr;
      block.ConsumeWhitespace();
      if (!block.AtEnd())
        return nullptr;
      selector->SetNth(ab.first, ab.second);
      return selector;
    }
    case CSSSelector::kPseudoHighlight: {
      const CSSParserToken& ident = block.ConsumeIncludingWhitespace();
      if (ident.GetType() != kIdentToken || !block.AtEnd())
        return nullptr;
      selector->SetArgument(ident.Value().ToAtomicString());
      return selector;
    }
    case CSSSelector::kPseudoToggle: {
      using State = ToggleRoot::State;

      const CSSParserToken& name = block.ConsumeIncludingWhitespace();
      if (name.GetType() != kIdentToken ||
          !css_parsing_utils::IsCustomIdent(name.Id())) {
        return nullptr;
      }
      std::unique_ptr<State> value;
      if (!block.AtEnd()) {
        const CSSParserToken& value_token = block.ConsumeIncludingWhitespace();
        switch (value_token.GetType()) {
          case kIdentToken:
            if (!css_parsing_utils::IsCustomIdent(value_token.Id()))
              return nullptr;
            value =
                std::make_unique<State>(value_token.Value().ToAtomicString());
            break;
          case kNumberToken:
            if (value_token.GetNumericValueType() != kIntegerValueType ||
                value_token.NumericValue() < 0) {
              return nullptr;
            }
            value = std::make_unique<State>(value_token.NumericValue());
            break;
          default:
            return nullptr;
        }
      }
      if (!block.AtEnd())
        return nullptr;
      selector->SetToggle(name.Value().ToAtomicString(), std::move(value));
      return selector;
    }
    default:
      break;
  }

  return nullptr;
}

CSSSelector::RelationType CSSSelectorParser::ConsumeCombinator(
    CSSParserTokenRange& range) {
  CSSSelector::RelationType fallback_result = CSSSelector::kSubSelector;
  while (range.Peek().GetType() == kWhitespaceToken) {
    range.Consume();
    fallback_result = CSSSelector::kDescendant;
  }

  if (range.Peek().GetType() != kDelimiterToken)
    return fallback_result;

  switch (range.Peek().Delimiter()) {
    case '+':
      range.ConsumeIncludingWhitespace();
      return CSSSelector::kDirectAdjacent;

    case '~':
      range.ConsumeIncludingWhitespace();
      return CSSSelector::kIndirectAdjacent;

    case '>':
      range.ConsumeIncludingWhitespace();
      return CSSSelector::kChild;

    default:
      break;
  }
  return fallback_result;
}

CSSSelector::MatchType CSSSelectorParser::ConsumeAttributeMatch(
    CSSParserTokenRange& range) {
  const CSSParserToken& token = range.ConsumeIncludingWhitespace();
  switch (token.GetType()) {
    case kIncludeMatchToken:
      return CSSSelector::kAttributeList;
    case kDashMatchToken:
      return CSSSelector::kAttributeHyphen;
    case kPrefixMatchToken:
      return CSSSelector::kAttributeBegin;
    case kSuffixMatchToken:
      return CSSSelector::kAttributeEnd;
    case kSubstringMatchToken:
      return CSSSelector::kAttributeContain;
    case kDelimiterToken:
      if (token.Delimiter() == '=')
        return CSSSelector::kAttributeExact;
      [[fallthrough]];
    default:
      failed_parsing_ = true;
      return CSSSelector::kAttributeExact;
  }
}

CSSSelector::AttributeMatchType CSSSelectorParser::ConsumeAttributeFlags(
    CSSParserTokenRange& range) {
  if (range.Peek().GetType() != kIdentToken)
    return CSSSelector::AttributeMatchType::kCaseSensitive;
  const CSSParserToken& flag = range.ConsumeIncludingWhitespace();
  if (EqualIgnoringASCIICase(flag.Value(), "i"))
    return CSSSelector::AttributeMatchType::kCaseInsensitive;
  else if (EqualIgnoringASCIICase(flag.Value(), "s") &&
           RuntimeEnabledFeatures::CSSCaseSensitiveSelectorEnabled())
    return CSSSelector::AttributeMatchType::kCaseSensitiveAlways;
  failed_parsing_ = true;
  return CSSSelector::AttributeMatchType::kCaseSensitive;
}

bool CSSSelectorParser::ConsumeANPlusB(CSSParserTokenRange& range,
                                       std::pair<int, int>& result) {
  const CSSParserToken& token = range.Consume();
  if (token.GetType() == kNumberToken &&
      token.GetNumericValueType() == kIntegerValueType) {
    result = std::make_pair(0, ClampTo<int>(token.NumericValue()));
    return true;
  }
  if (token.GetType() == kIdentToken) {
    if (EqualIgnoringASCIICase(token.Value(), "odd")) {
      result = std::make_pair(2, 1);
      return true;
    }
    if (EqualIgnoringASCIICase(token.Value(), "even")) {
      result = std::make_pair(2, 0);
      return true;
    }
  }

  // The 'n' will end up as part of an ident or dimension. For a valid <an+b>,
  // this will store a string of the form 'n', 'n-', or 'n-123'.
  String n_string;

  if (token.GetType() == kDelimiterToken && token.Delimiter() == '+' &&
      range.Peek().GetType() == kIdentToken) {
    result.first = 1;
    n_string = range.Consume().Value().ToString();
  } else if (token.GetType() == kDimensionToken &&
             token.GetNumericValueType() == kIntegerValueType) {
    result.first = ClampTo<int>(token.NumericValue());
    n_string = token.Value().ToString();
  } else if (token.GetType() == kIdentToken) {
    if (token.Value()[0] == '-') {
      result.first = -1;
      n_string = token.Value().ToString().Substring(1);
    } else {
      result.first = 1;
      n_string = token.Value().ToString();
    }
  }

  range.ConsumeWhitespace();

  if (n_string.IsEmpty() || !IsASCIIAlphaCaselessEqual(n_string[0], 'n'))
    return false;
  if (n_string.length() > 1 && n_string[1] != '-')
    return false;

  if (n_string.length() > 2) {
    bool valid;
    result.second = n_string.Substring(1).ToIntStrict(&valid);
    return valid;
  }

  NumericSign sign = n_string.length() == 1 ? kNoSign : kMinusSign;
  if (sign == kNoSign && range.Peek().GetType() == kDelimiterToken) {
    char delimiter_sign = range.ConsumeIncludingWhitespace().Delimiter();
    if (delimiter_sign == '+')
      sign = kPlusSign;
    else if (delimiter_sign == '-')
      sign = kMinusSign;
    else
      return false;
  }

  if (sign == kNoSign && range.Peek().GetType() != kNumberToken) {
    result.second = 0;
    return true;
  }

  const CSSParserToken& b = range.Consume();
  if (b.GetType() != kNumberToken ||
      b.GetNumericValueType() != kIntegerValueType)
    return false;
  if ((b.GetNumericSign() == kNoSign) == (sign == kNoSign))
    return false;
  result.second = ClampTo<int>(b.NumericValue());
  if (sign == kMinusSign) {
    // Negating minimum integer returns itself, instead return max integer.
    if (UNLIKELY(result.second == std::numeric_limits<int>::min()))
      result.second = std::numeric_limits<int>::max();
    else
      result.second = -result.second;
  }
  return true;
}

const AtomicString& CSSSelectorParser::DefaultNamespace() const {
  if (!style_sheet_ || ignore_default_namespace_)
    return g_star_atom;
  return style_sheet_->DefaultNamespace();
}

const AtomicString& CSSSelectorParser::DetermineNamespace(
    const AtomicString& prefix) {
  if (prefix.IsNull())
    return DefaultNamespace();
  if (prefix.IsEmpty())
    return g_empty_atom;  // No namespace. If an element/attribute has a
                          // namespace, we won't match it.
  if (prefix == g_star_atom)
    return g_star_atom;  // We'll match any namespace.
  if (!style_sheet_)
    return g_null_atom;  // Cannot resolve prefix to namespace without a
                         // stylesheet, syntax error.
  return style_sheet_->NamespaceURIFromPrefix(prefix);
}

void CSSSelectorParser::PrependTypeSelectorIfNeeded(
    const AtomicString& namespace_prefix,
    bool has_q_name,
    const AtomicString& element_name,
    CSSParserSelector* compound_selector) {
  if (!has_q_name && DefaultNamespace() == g_star_atom &&
      !compound_selector->NeedsImplicitShadowCombinatorForMatching())
    return;

  AtomicString determined_element_name =
      !has_q_name ? CSSSelector::UniversalSelectorAtom() : element_name;
  AtomicString namespace_uri = DetermineNamespace(namespace_prefix);
  if (namespace_uri.IsNull()) {
    failed_parsing_ = true;
    return;
  }
  AtomicString determined_prefix = namespace_prefix;
  if (namespace_uri == DefaultNamespace())
    determined_prefix = g_null_atom;
  QualifiedName tag =
      QualifiedName(determined_prefix, determined_element_name, namespace_uri);

  // *:host/*:host-context never matches, so we can't discard the *,
  // otherwise we can't tell the difference between *:host and just :host.
  //
  // Also, selectors where we use a ShadowPseudo combinator between the
  // element and the pseudo element for matching (custom pseudo elements,
  // ::cue, ::shadow), we need a universal selector to set the combinator
  // (relation) on in the cases where there are no simple selectors preceding
  // the pseudo element.
  bool is_host_pseudo = compound_selector->IsHostPseudoSelector();
  if (is_host_pseudo && !has_q_name && namespace_prefix.IsNull())
    return;
  if (tag != AnyQName() || is_host_pseudo ||
      compound_selector->NeedsImplicitShadowCombinatorForMatching()) {
    compound_selector->PrependTagSelector(
        tag,
        determined_prefix == g_null_atom &&
            determined_element_name == CSSSelector::UniversalSelectorAtom() &&
            !is_host_pseudo);
  }
}

std::unique_ptr<CSSParserSelector>
CSSSelectorParser::AddSimpleSelectorToCompound(
    std::unique_ptr<CSSParserSelector> compound_selector,
    std::unique_ptr<CSSParserSelector> simple_selector) {
  compound_selector->AppendTagHistory(CSSSelector::kSubSelector,
                                      std::move(simple_selector));
  return compound_selector;
}

std::unique_ptr<CSSParserSelector>
CSSSelectorParser::SplitCompoundAtImplicitShadowCrossingCombinator(
    std::unique_ptr<CSSParserSelector> compound_selector) {
  // The tagHistory is a linked list that stores combinator separated compound
  // selectors from right-to-left. Yet, within a single compound selector,
  // stores the simple selectors from left-to-right.
  //
  // ".a.b > div#id" is stored in a tagHistory as [div, #id, .a, .b], each
  // element in the list stored with an associated relation (combinator or
  // SubSelector).
  //
  // ::cue, ::shadow, and custom pseudo elements have an implicit ShadowPseudo
  // combinator to their left, which really makes for a new compound selector,
  // yet it's consumed by the selector parser as a single compound selector.
  //
  // Example:
  //
  // input#x::-webkit-clear-button -> [ ::-webkit-clear-button, input, #x ]
  //
  // Likewise, ::slotted() pseudo element has an implicit ShadowSlot combinator
  // to its left for finding matching slot element in other TreeScope.
  //
  // ::part has a implicit ShadowPart combinator to it's left finding the host
  // element in the scope of the style rule.
  //
  // Example:
  //
  // slot[name=foo]::slotted(div) -> [ ::slotted(div), slot, [name=foo] ]
  CSSParserSelector* split_after = compound_selector.get();

  while (split_after->TagHistory() &&
         !split_after->TagHistory()->NeedsImplicitShadowCombinatorForMatching())
    split_after = split_after->TagHistory();

  if (!split_after || !split_after->TagHistory())
    return compound_selector;

  std::unique_ptr<CSSParserSelector> remaining =
      split_after->ReleaseTagHistory();
  CSSSelector::RelationType relation =
      remaining->GetImplicitShadowCombinatorForMatching();
  // We might need to split the compound twice since ::placeholder is allowed
  // after ::slotted and they both need an implicit combinator for matching.
  remaining =
      SplitCompoundAtImplicitShadowCrossingCombinator(std::move(remaining));
  remaining->AppendTagHistory(relation, std::move(compound_selector));
  return remaining;
}

namespace {

struct PseudoElementFeatureMapEntry {
  template <unsigned key_length>
  PseudoElementFeatureMapEntry(const char (&key)[key_length],
                               WebFeature feature)
      : key(key),
        key_length(base::checked_cast<uint16_t>(key_length - 1)),
        feature(base::checked_cast<uint16_t>(feature)) {}
  const char* const key;
  const uint16_t key_length;
  const uint16_t feature;
};

WebFeature FeatureForWebKitCustomPseudoElement(const AtomicString& name) {
  static const PseudoElementFeatureMapEntry feature_table[] = {
      {"cue", WebFeature::kCSSSelectorCue},
      {"-internal-media-controls-overlay-cast-button",
       WebFeature::kCSSSelectorInternalMediaControlsOverlayCastButton},
      {"-webkit-calendar-picker-indicator",
       WebFeature::kCSSSelectorWebkitCalendarPickerIndicator},
      {"-webkit-clear-button", WebFeature::kCSSSelectorWebkitClearButton},
      {"-webkit-color-swatch", WebFeature::kCSSSelectorWebkitColorSwatch},
      {"-webkit-color-swatch-wrapper",
       WebFeature::kCSSSelectorWebkitColorSwatchWrapper},
      {"-webkit-date-and-time-value",
       WebFeature::kCSSSelectorWebkitDateAndTimeValue},
      {"-webkit-datetime-edit", WebFeature::kCSSSelectorWebkitDatetimeEdit},
      {"-webkit-datetime-edit-ampm-field",
       WebFeature::kCSSSelectorWebkitDatetimeEditAmpmField},
      {"-webkit-datetime-edit-day-field",
       WebFeature::kCSSSelectorWebkitDatetimeEditDayField},
      {"-webkit-datetime-edit-fields-wrapper",
       WebFeature::kCSSSelectorWebkitDatetimeEditFieldsWrapper},
      {"-webkit-datetime-edit-hour-field",
       WebFeature::kCSSSelectorWebkitDatetimeEditHourField},
      {"-webkit-datetime-edit-millisecond-field",
       WebFeature::kCSSSelectorWebkitDatetimeEditMillisecondField},
      {"-webkit-datetime-edit-minute-field",
       WebFeature::kCSSSelectorWebkitDatetimeEditMinuteField},
      {"-webkit-datetime-edit-month-field",
       WebFeature::kCSSSelectorWebkitDatetimeEditMonthField},
      {"-webkit-datetime-edit-second-field",
       WebFeature::kCSSSelectorWebkitDatetimeEditSecondField},
      {"-webkit-datetime-edit-text",
       WebFeature::kCSSSelectorWebkitDatetimeEditText},
      {"-webkit-datetime-edit-week-field",
       WebFeature::kCSSSelectorWebkitDatetimeEditWeekField},
      {"-webkit-datetime-edit-year-field",
       WebFeature::kCSSSelectorWebkitDatetimeEditYearField},
      {"-webkit-file-upload-button",
       WebFeature::kCSSSelectorWebkitFileUploadButton},
      {"-webkit-inner-spin-button",
       WebFeature::kCSSSelectorWebkitInnerSpinButton},
      {"-webkit-input-placeholder",
       WebFeature::kCSSSelectorWebkitInputPlaceholder},
      {"-webkit-media-controls", WebFeature::kCSSSelectorWebkitMediaControls},
      {"-webkit-media-controls-current-time-display",
       WebFeature::kCSSSelectorWebkitMediaControlsCurrentTimeDisplay},
      {"-webkit-media-controls-enclosure",
       WebFeature::kCSSSelectorWebkitMediaControlsEnclosure},
      {"-webkit-media-controls-fullscreen-button",
       WebFeature::kCSSSelectorWebkitMediaControlsFullscreenButton},
      {"-webkit-media-controls-mute-button",
       WebFeature::kCSSSelectorWebkitMediaControlsMuteButton},
      {"-webkit-media-controls-overlay-enclosure",
       WebFeature::kCSSSelectorWebkitMediaControlsOverlayEnclosure},
      {"-webkit-media-controls-overlay-play-button",
       WebFeature::kCSSSelectorWebkitMediaControlsOverlayPlayButton},
      {"-webkit-media-controls-panel",
       WebFeature::kCSSSelectorWebkitMediaControlsPanel},
      {"-webkit-media-controls-play-button",
       WebFeature::kCSSSelectorWebkitMediaControlsPlayButton},
      {"-webkit-media-controls-timeline",
       WebFeature::kCSSSelectorWebkitMediaControlsTimeline},
      // Note: This feature is no longer implemented in Blink.
      {"-webkit-media-controls-timeline-container",
       WebFeature::kCSSSelectorWebkitMediaControlsTimelineContainer},
      {"-webkit-media-controls-time-remaining-display",
       WebFeature::kCSSSelectorWebkitMediaControlsTimeRemainingDisplay},
      {"-webkit-media-controls-toggle-closed-captions-button",
       WebFeature::kCSSSelectorWebkitMediaControlsToggleClosedCaptionsButton},
      {"-webkit-media-controls-volume-slider",
       WebFeature::kCSSSelectorWebkitMediaControlsVolumeSlider},
      {"-webkit-media-slider-container",
       WebFeature::kCSSSelectorWebkitMediaSliderContainer},
      {"-webkit-media-slider-thumb",
       WebFeature::kCSSSelectorWebkitMediaSliderThumb},
      {"-webkit-media-text-track-container",
       WebFeature::kCSSSelectorWebkitMediaTextTrackContainer},
      {"-webkit-media-text-track-display",
       WebFeature::kCSSSelectorWebkitMediaTextTrackDisplay},
      {"-webkit-media-text-track-region",
       WebFeature::kCSSSelectorWebkitMediaTextTrackRegion},
      {"-webkit-media-text-track-region-container",
       WebFeature::kCSSSelectorWebkitMediaTextTrackRegionContainer},
      {"-webkit-meter-bar", WebFeature::kCSSSelectorWebkitMeterBar},
      {"-webkit-meter-even-less-good-value",
       WebFeature::kCSSSelectorWebkitMeterEvenLessGoodValue},
      {"-webkit-meter-inner-element",
       WebFeature::kCSSSelectorWebkitMeterInnerElement},
      {"-webkit-meter-optimum-value",
       WebFeature::kCSSSelectorWebkitMeterOptimumValue},
      {"-webkit-meter-suboptimum-value",
       WebFeature::kCSSSelectorWebkitMeterSuboptimumValue},
      {"-webkit-progress-bar", WebFeature::kCSSSelectorWebkitProgressBar},
      {"-webkit-progress-inner-element",
       WebFeature::kCSSSelectorWebkitProgressInnerElement},
      {"-webkit-progress-value", WebFeature::kCSSSelectorWebkitProgressValue},
      {"-webkit-search-cancel-button",
       WebFeature::kCSSSelectorWebkitSearchCancelButton},
      {"-webkit-slider-container",
       WebFeature::kCSSSelectorWebkitSliderContainer},
      {"-webkit-slider-runnable-track",
       WebFeature::kCSSSelectorWebkitSliderRunnableTrack},
      {"-webkit-slider-thumb", WebFeature::kCSSSelectorWebkitSliderThumb},
      {"-webkit-textfield-decoration-container",
       WebFeature::kCSSSelectorWebkitTextfieldDecorationContainer},
  };
  // TODO(fs): Could use binary search once there's a less finicky way to
  // compare (order) String and StringView/non-String.
  for (const auto& entry : feature_table) {
    if (name == StringView(entry.key, entry.key_length))
      return static_cast<WebFeature>(entry.feature);
  }
  return WebFeature::kCSSSelectorWebkitUnknownPseudo;
}

}  // namespace

static void RecordUsageAndDeprecationsOneSelector(
    const CSSSelector* selector,
    const CSSParserContext* context) {
  WebFeature feature = WebFeature::kNumberOfFeatures;
  switch (selector->GetPseudoType()) {
    case CSSSelector::kPseudoAny:
      feature = WebFeature::kCSSSelectorPseudoAny;
      break;
    case CSSSelector::kPseudoIs:
      feature = WebFeature::kCSSSelectorPseudoIs;
      break;
    case CSSSelector::kPseudoFocusVisible:
      DCHECK(RuntimeEnabledFeatures::CSSFocusVisibleEnabled());
      feature = WebFeature::kCSSSelectorPseudoFocusVisible;
      break;
    case CSSSelector::kPseudoFocus:
      feature = WebFeature::kCSSSelectorPseudoFocus;
      break;
    case CSSSelector::kPseudoAnyLink:
      feature = WebFeature::kCSSSelectorPseudoAnyLink;
      break;
    case CSSSelector::kPseudoWebkitAnyLink:
      feature = WebFeature::kCSSSelectorPseudoWebkitAnyLink;
      break;
    case CSSSelector::kPseudoWhere:
      feature = WebFeature::kCSSSelectorPseudoWhere;
      break;
    case CSSSelector::kPseudoDefined:
      feature = WebFeature::kCSSSelectorPseudoDefined;
      break;
    case CSSSelector::kPseudoSlotted:
      feature = WebFeature::kCSSSelectorPseudoSlotted;
      break;
    case CSSSelector::kPseudoHost:
      feature = WebFeature::kCSSSelectorPseudoHost;
      break;
    case CSSSelector::kPseudoHostContext:
      feature = WebFeature::kCSSSelectorPseudoHostContext;
      break;
    case CSSSelector::kPseudoFullScreenAncestor:
      feature = WebFeature::kCSSSelectorPseudoFullScreenAncestor;
      break;
    case CSSSelector::kPseudoFullScreen:
      feature = WebFeature::kCSSSelectorPseudoFullScreen;
      break;
    case CSSSelector::kPseudoListBox:
      feature = WebFeature::kCSSSelectorInternalPseudoListBox;
      break;
    case CSSSelector::kPseudoWebKitCustomElement:
      feature = FeatureForWebKitCustomPseudoElement(selector->Value());
      break;
    case CSSSelector::kPseudoSpatialNavigationFocus:
      feature = WebFeature::kCSSSelectorInternalPseudoSpatialNavigationFocus;
      break;
    case CSSSelector::kPseudoReadOnly:
      feature = WebFeature::kCSSSelectorPseudoReadOnly;
      break;
    case CSSSelector::kPseudoReadWrite:
      feature = WebFeature::kCSSSelectorPseudoReadWrite;
      break;
    case CSSSelector::kPseudoDir:
      DCHECK(RuntimeEnabledFeatures::CSSPseudoDirEnabled());
      feature = WebFeature::kCSSSelectorPseudoDir;
      break;
    case CSSSelector::kPseudoHas:
      DCHECK(RuntimeEnabledFeatures::CSSPseudoHasEnabled());
      if (context->IsLiveProfile())
        feature = WebFeature::kCSSSelectorPseudoHasInLiveProfile;
      else
        feature = WebFeature::kCSSSelectorPseudoHasInSnapshotProfile;
      break;
    default:
      break;
  }
  if (feature != WebFeature::kNumberOfFeatures) {
    if (Deprecation::IsDeprecated(feature)) {
      context->CountDeprecation(feature);
    } else {
      context->Count(feature);
    }
  }
  if (selector->Relation() == CSSSelector::kIndirectAdjacent)
    context->Count(WebFeature::kCSSSelectorIndirectAdjacent);
  if (selector->SelectorList()) {
    for (const CSSSelector* current = selector->SelectorList()->First();
         current; current = current->TagHistory()) {
      RecordUsageAndDeprecationsOneSelector(current, context);
    }
  }
}

void CSSSelectorParser::RecordUsageAndDeprecations(
    const CSSSelectorVector& selector_vector) {
  if (!context_->IsUseCounterRecordingEnabled())
    return;
  if (context_->Mode() == kUASheetMode)
    return;

  for (const std::unique_ptr<CSSParserSelector>& selector : selector_vector) {
    for (const CSSParserSelector* current = selector.get(); current;
         current = current->TagHistory()) {
      RecordUsageAndDeprecationsOneSelector(current->GetSelector(), context_);
    }
  }
}

bool CSSSelectorParser::ContainsUnknownWebkitPseudoElements(
    const CSSSelector& complex_selector) {
  for (const CSSSelector* current = &complex_selector; current;
       current = current->TagHistory()) {
    if (current->GetPseudoType() != CSSSelector::kPseudoWebKitCustomElement)
      continue;
    WebFeature feature = FeatureForWebKitCustomPseudoElement(current->Value());
    if (feature == WebFeature::kCSSSelectorWebkitUnknownPseudo)
      return true;
  }
  return false;
}

}  // namespace blink
