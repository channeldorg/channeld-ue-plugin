#pragma once
#include "IDetailCustomization.h"

class FChanneldSettingsDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	virtual void CustomizeDetails(const TSharedPtr<IDetailLayoutBuilder>& DetailBuilder) override;

protected:
	TWeakPtr<IDetailLayoutBuilder> CachedDetailBuilder;
};
