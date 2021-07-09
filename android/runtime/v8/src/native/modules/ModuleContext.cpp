/**
 * Appcelerator Titanium Mobile
 * Copyright (c) 2021 by Axway, Inc. All Rights Reserved.
 * Licensed under the terms of the Apache Public License
 * Please see the LICENSE included with this distribution for details.
 */
#include <v8.h>
#include <v8-util.h>

#include "AndroidUtil.h"
#include "ModuleContext.h"
#include "V8Util.h"

#define TAG "ModuleContext"

namespace titanium {
	using namespace v8;

	std::vector<Persistent<Value, CopyablePersistentTraits<Value>>> ModuleContext::persistentBuiltins;

	Local<Name> Uint32ToName(Local<Context> context, uint32_t index)
	{
		return Uint32::New(context->GetIsolate(), index)->ToString(context).ToLocalChecked();
	}

	Local<Object> GetPrototype(Local<Context> context, Local<Object> object)
	{
		Local<Value> prototype;
		if (!object.IsEmpty()) {
			object->GetRealNamedProperty(context, STRING_NEW(context->GetIsolate(), "prototype")).ToLocal(&prototype);
		}
		return !prototype.IsEmpty() && prototype->IsObject() ? prototype.As<Object>() : Local<Object>();
	}

	bool ModuleContext::SetHandlersEnabled(Local<Context> context, bool enabled)
	{
		bool current = true;
		if (context->GetNumberOfEmbedderDataFields() > 0) {
			Local<Array> data = context->GetEmbedderData(EmbedderIndex::kData).As<Array>();
			current = data->Get(context, DataIndex::kHandlersEnabled).ToLocalChecked().As<Boolean>()->Value();
			data->Set(context, DataIndex::kHandlersEnabled, Boolean::New(context->GetIsolate(), enabled));
		}
		return current;
	}

	void ModuleContext::ReadBuiltins(Local<Context> context)
	{
		Isolate* isolate = context->GetIsolate();
		Local<Object> global = context->Global();
		Local<Object> prototype = global->GetPrototype().As<Object>();

		// Obtain context builtins.
		Local<Array> builtinNames = prototype->GetOwnPropertyNames(
			context,
			static_cast<v8::PropertyFilter>(ALL_PROPERTIES),
			KeyConversionMode::kConvertToString).FromMaybe(Local<Array>()
		);
		if (builtinNames.IsEmpty()) {
			return;
		}

		Local<Object> builtins = Object::New(isolate);
		int builtinNamesLength = builtinNames->Length();

		for (int i = 0; i < builtinNamesLength; i++) {
			Local<String> key = builtinNames->Get(context, i).ToLocalChecked().As<String>();
			MaybeLocal<Value> builtinValue = global->GetRealNamedProperty(context, key);

			if (!builtinValue.IsEmpty()) {
				Local<Value> builtin = builtinValue.ToLocalChecked();

				if (!builtin.IsEmpty() && builtin->IsObject()) {
					Local<Object> prototype = GetPrototype(context, builtin.As<Object>());

					if (!prototype.IsEmpty()) {
						Local<Array> prototypeNames = prototype->GetOwnPropertyNames(
							context,
							static_cast<v8::PropertyFilter>(ALL_PROPERTIES),
							KeyConversionMode::kConvertToString
						).FromMaybe(Local<Array>());

						if (!prototypeNames.IsEmpty()) {
							Local<Object> prototypeDescriptors = Object::New(isolate);
							int prototypeNamesLength = prototypeNames->Length();

							for (int i = 0; i < prototypeNamesLength; i++) {
								Local<String> key = prototypeNames->Get(context, i).ToLocalChecked().As<String>();

								MaybeLocal<Value> maybeDescriptor = prototype->GetOwnPropertyDescriptor(context, key);
								if (!maybeDescriptor.IsEmpty()) {
									Local<Object> descriptorObject = maybeDescriptor.ToLocalChecked().As<Object>();
									std::unique_ptr<PropertyDescriptor> descriptor(V8Util::objectToDescriptor(context, descriptorObject));

									if (descriptor->writable()) {
										prototypeDescriptors->CreateDataProperty(context, key, descriptorObject);

										Persistent<Value> persistentDescriptor(isolate, descriptorObject);
										ModuleContext::persistentBuiltins.emplace_back(persistentDescriptor);
									}
								}
							}

							builtins->CreateDataProperty(context, key, prototypeDescriptors);
						}
					}
				}
			}
		}

		// Remove context specific builtins from the list.
		Local<String> GLOBAL_THIS = NEW_SYMBOL(isolate, "globalThis");
		Local<String> CONSOLE = NEW_SYMBOL(isolate, "console");

		builtins->Delete(context, GLOBAL_THIS);
		builtins->Delete(context, CONSOLE);

		context->SetEmbedderData(EmbedderIndex::kBuiltins, builtins);
	}

	void ModuleContext::CopyPrototypes(Local<Context> sourceContext, Local<Context> destinationContext)
	{
		Isolate* isolate = destinationContext->GetIsolate();
		Local<Context> context;

		if (sourceContext->GetNumberOfEmbedderDataFields() > 0) {
			context = sourceContext;
		} else if (destinationContext->GetNumberOfEmbedderDataFields() > 0) {
			context = destinationContext;
		} else {
			return;
		}

		Local<Value> builtinsValue = context->GetEmbedderData(EmbedderIndex::kBuiltins);
		if (builtinsValue.IsEmpty() || !builtinsValue->IsObject()) {
			return;
		}
		Local<Object> builtins = builtinsValue.As<Object>();
		if (builtins.IsEmpty()) {
			return;
		}

		// Temporarily disable context callback handlers.
		bool sourceHandlersEnabled = SetHandlersEnabled(sourceContext, false);
		bool destinationHandlersEnabled = SetHandlersEnabled(destinationContext, false);

		Local<Array> builtinNames = builtins->GetOwnPropertyNames(context).ToLocalChecked();
		int builtinNamesLength = builtinNames->Length();

		LOGE(TAG, "%s", *Utf8Value(context->Global()->Get(context, STRING_NEW(isolate, "__filename")).ToLocalChecked().As<String>()));

		for (int i = 0; i < builtinNamesLength; i++) {
			Local<String> key = builtinNames->Get(context, i).ToLocalChecked().As<String>();
			Local<Object> builtinPrototype = builtins->GetRealNamedProperty(context, key).ToLocalChecked().As<Object>();

			Local<Object> sourceBuiltins = sourceContext->Global();
			Local<Object> destinationBuiltins = destinationContext->Global();

			if (destinationBuiltins->Has(context, key).FromMaybe(false)) {
				Local<Value> sourceBuiltinValue = sourceBuiltins->GetRealNamedProperty(context, key).ToLocalChecked();
				Local<Value> destinationBuiltinValue = destinationBuiltins->GetRealNamedProperty(context, key).ToLocalChecked();

				if (!sourceBuiltinValue.IsEmpty() && !destinationBuiltinValue.IsEmpty()) {
					Local<Object> sourceBuiltinPrototype = GetPrototype(context, sourceBuiltinValue.As<Object>());
					Local<Object> destinationBuiltinPrototype = GetPrototype(context, destinationBuiltinValue.As<Object>());

					LOGE(TAG, "CopyPrototypes %s", *Utf8Value(key));

					if (!sourceBuiltinPrototype.IsEmpty() && !destinationBuiltinPrototype.IsEmpty()) {

						Local<Array> sourceProperties = sourceBuiltinPrototype->GetOwnPropertyNames(
							context,
							static_cast<v8::PropertyFilter>(ALL_PROPERTIES),
							KeyConversionMode::kConvertToString
						).FromMaybe(Local<Array>());

						if (!sourceProperties.IsEmpty()) {
							int sourcePropertiesLength = sourceProperties->Length();

							for (int i = 0; i < sourcePropertiesLength; i++) {
								Local<String> key = sourceProperties->Get(context, i).ToLocalChecked().As<String>();

								if (key->IsSymbol()) {
									continue;
								}

								MaybeLocal<Value> sourceDescriptorValue = sourceBuiltinPrototype->GetOwnPropertyDescriptor(context, key);
								if (sourceDescriptorValue.IsEmpty()) {
									continue;
								}
								std::unique_ptr<PropertyDescriptor> sourceDescriptor(V8Util::objectToDescriptor(context, sourceDescriptorValue.ToLocalChecked().As<Object>()));

								if (destinationBuiltinPrototype->HasRealNamedProperty(context, key).FromMaybe(false)) {
									bool skip = true;

									MaybeLocal<Value> destinationDescriptorValue = destinationBuiltinPrototype->GetOwnPropertyDescriptor(context, key);
									if (destinationDescriptorValue.IsEmpty()) {
										continue;
									}
									std::unique_ptr<PropertyDescriptor> destinationDescriptor(V8Util::objectToDescriptor(context, destinationDescriptorValue.ToLocalChecked().As<Object>()));

									if (!destinationDescriptor->writable()) {
										continue;
									}

									if (context != sourceContext) {
										continue;
									}

									if (builtinPrototype->HasRealNamedProperty(context, key).FromMaybe(false)) {
										MaybeLocal<Value> builtinDescriptorValue = builtinPrototype->GetRealNamedProperty(context, key);

										if (!builtinDescriptorValue.IsEmpty()) {
											std::unique_ptr<PropertyDescriptor> builtinDescriptor(V8Util::objectToDescriptor(context, builtinDescriptorValue.ToLocalChecked().As<Object>()));

											/*for (const auto& p : persistentBuiltins) {
												if (sourceDescriptorValue.ToLocalChecked()->Equals(context, p.Get(isolate)).FromMaybe(false)) {
													LOGE(TAG, "SAME");
												}
												/*std::unique_ptr<PropertyDescriptor> d(V8Util::objectToDescriptor(context, p.Get(isolate).As<Object>()));

												if (d->value()->Equals(context, sourceDescriptor->value()).FromMaybe(false)) {
													LOGE(TAG, "CONTAINS INITIAL BUILTIN!");
												}*
											}*/

											if (sourceDescriptor->has_get() && builtinDescriptor->has_get()) {
												if (!sourceDescriptor->get()->Equals(context, builtinDescriptor->get()).FromMaybe(false)) {
													LOGE(TAG, "has_get() - %s", *Utf8Value(key));
													skip = false;
												}
											} else if (sourceDescriptor->has_set() && builtinDescriptor->has_set()) {
												if (!sourceDescriptor->set()->Equals(context, builtinDescriptor->set()).FromMaybe(false)) {
													LOGE(TAG, "has_set() - %s", *Utf8Value(key));
													skip = false;
												}
											} else if (sourceDescriptor->has_value() && builtinDescriptor->has_value()) {
												if (!sourceDescriptor->value()->Equals(context, builtinDescriptor->value()).FromMaybe(false)) {
													LOGE(TAG, "value() - %s", *Utf8Value(key));
													// skip = false;
												}
											}
										}
									}

									if (skip) {
										continue;
									}
								}

								LOGE(TAG, "  %s", *Utf8Value(key));
								destinationBuiltinPrototype->DefineProperty(context, key, *sourceDescriptor);
							}
						}
					}
				}
			}
		}

		// Restore context callback handler state.
		SetHandlersEnabled(sourceContext, sourceHandlersEnabled);
		SetHandlersEnabled(destinationContext, destinationHandlersEnabled);
	}

	Local<Context> ModuleContext::New(Local<Context> context, Local<Object> extensions)
	{
		Isolate *isolate = context->GetIsolate();
		EscapableHandleScope scope(isolate);

		// Create template for callbacks.
		Local<ObjectTemplate> callbackTemplate = ObjectTemplate::New(isolate);

		// Setup data array for handlers.
		Local<Array> data = Array::New(isolate, 3);

		// Determine if handlers should be active.
		data->Set(context, DataIndex::kHandlersEnabled, Boolean::New(isolate, false));
		data->Set(context, DataIndex::kParentGlobal, context->Global());
		data->Set(context, DataIndex::kExtensions, extensions);

		callbackTemplate->SetHandler(
			NamedPropertyHandlerConfiguration(
				GetterCallback,
				SetterCallback,
				QueryCallback,
				DeleterCallback,
				EnumeratorCallback,
				DefinerCallback,
				DescriptorCallback,
				data
			)
		);
		callbackTemplate->SetHandler(
			IndexedPropertyHandlerConfiguration(
				IndexedGetterCallback,
				IndexedSetterCallback,
				IndexedQueryCallback,
				IndexedDeleterCallback,
				IndexedEnumeratorCallback,
				IndexedDefinerCallback,
				IndexedDescriptorCallback,
				data
			)
		);

		// Create new module context with callback template.
		Local<Context> moduleContext = Context::New(isolate, nullptr, callbackTemplate);

		// Use same security token as parent context.
		moduleContext->SetSecurityToken(context->GetSecurityToken());

		// Embed builtins.
		ReadBuiltins(moduleContext);

		// Embed data array.
		moduleContext->SetEmbedderData(EmbedderIndex::kData, data);

		// Extend module context with extensions.
		if (!extensions.IsEmpty()) {
			Local<Object> moduleGlobal = moduleContext->Global();
			V8Util::objectExtend(moduleGlobal, extensions);
		}

		// Activate handlers.
		data->Set(context, DataIndex::kHandlersEnabled, Boolean::New(isolate, true));

		return scope.Escape(moduleContext);
	}

	void ModuleContext::GetterCallback(Local<Name> property, const PropertyCallbackInfo<Value>& info)
	{
		Isolate* isolate = info.GetIsolate();
		Local<Context> context = isolate->GetCurrentContext();

		Local<Array> data = info.Data().As<Array>();
		Local<Boolean> handlersEnabled = data->Get(context, DataIndex::kHandlersEnabled).ToLocalChecked().As<Boolean>();
		Local<Object> parentGlobal = data->Get(context, DataIndex::kParentGlobal).ToLocalChecked().As<Object>();
		Local<Object> extensions = data->Get(context, DataIndex::kExtensions).ToLocalChecked().As<Object>();

		if (!handlersEnabled->Value()) {
			return;
		}

		Local<Object> builtins = context->GetEmbedderData(EmbedderIndex::kBuiltins).As<Object>();
		if (builtins->HasRealNamedProperty(context, property).FromMaybe(false)) {
			return;
		}

		if (!extensions->HasRealNamedProperty(context, property).FromMaybe(false)
				&& parentGlobal->HasRealNamedProperty(context, property).FromMaybe(false)) {
			Local<Value> value = parentGlobal->GetRealNamedProperty(context, property).ToLocalChecked();

			// Property defined on parent global, return property.
			info.GetReturnValue().Set(value);
		}
	}

	void ModuleContext::SetterCallback(Local<Name> property, Local<Value> value, const PropertyCallbackInfo<Value>& info)
	{
		Isolate* isolate = info.GetIsolate();
		Local<Context> context = isolate->GetCurrentContext();

		Local<Array> data = info.Data().As<Array>();
		Local<Boolean> handlersEnabled = data->Get(context, DataIndex::kHandlersEnabled).ToLocalChecked().As<Boolean>();
		Local<Object> parentGlobal = data->Get(context, DataIndex::kParentGlobal).ToLocalChecked().As<Object>();
		Local<Object> extensions = data->Get(context, DataIndex::kExtensions).ToLocalChecked().As<Object>();

		if (!handlersEnabled->Value()) {
			return;
		}

		Local<Object> builtins = context->GetEmbedderData(EmbedderIndex::kBuiltins).As<Object>();
		if (builtins->HasRealNamedProperty(context, property).FromMaybe(false)) {
			return;
		}

		auto attributes = PropertyAttribute::None;

		if (!extensions->HasRealNamedProperty(context, property).FromMaybe(false)
				&& parentGlobal->GetRealNamedPropertyAttributes(context, property).To(&attributes)) {
			bool readOnly = static_cast<int>(attributes) & static_cast<int>(PropertyAttribute::ReadOnly);

			if (!readOnly) {

				// Property defined on parent global, overwrite property.
				parentGlobal->CreateDataProperty(context, property, value);

				// Do not set on module global.
				info.GetReturnValue().Set(false);
			}
		}
	}

	void ModuleContext::QueryCallback(Local<Name> property, const PropertyCallbackInfo<Integer>& info)
	{
		Isolate* isolate = info.GetIsolate();
		Local<Context> context = isolate->GetCurrentContext();

		Local<Array> data = info.Data().As<Array>();
		Local<Boolean> handlersEnabled = data->Get(context, DataIndex::kHandlersEnabled).ToLocalChecked().As<Boolean>();
		Local<Object> parentGlobal = data->Get(context, DataIndex::kParentGlobal).ToLocalChecked().As<Object>();
		Local<Object> extensions = data->Get(context, DataIndex::kExtensions).ToLocalChecked().As<Object>();

		if (!handlersEnabled->Value()) {
			return;
		}

		Local<Object> builtins = context->GetEmbedderData(EmbedderIndex::kBuiltins).As<Object>();
		if (builtins->HasRealNamedProperty(context, property).FromMaybe(false)) {
			return;
		}

		auto attributes = PropertyAttribute::None;

		if (!extensions->HasRealNamedProperty(context, property).FromMaybe(false)
				&& parentGlobal->GetRealNamedPropertyAttributes(context, property).To(&attributes)) {

			// Property defined on parent global, return attributes.
			info.GetReturnValue().Set(attributes);
		}
	}

	void ModuleContext::DeleterCallback(Local<Name> property, const PropertyCallbackInfo<Boolean>& info)
	{
		Isolate* isolate = info.GetIsolate();
		Local<Context> context = isolate->GetCurrentContext();

		Local<Array> data = info.Data().As<Array>();
		Local<Boolean> handlersEnabled = data->Get(context, DataIndex::kHandlersEnabled).ToLocalChecked().As<Boolean>();
		Local<Object> parentGlobal = data->Get(context, DataIndex::kParentGlobal).ToLocalChecked().As<Object>();
		Local<Object> extensions = data->Get(context, DataIndex::kExtensions).ToLocalChecked().As<Object>();

		if (!handlersEnabled->Value()) {
			return;
		}

		Local<Object> builtins = context->GetEmbedderData(EmbedderIndex::kBuiltins).As<Object>();
		if (builtins->HasRealNamedProperty(context, property).FromMaybe(false)) {
			return;
		}

		if (!extensions->HasRealNamedProperty(context, property).FromMaybe(false)
				&& parentGlobal->HasRealNamedProperty(context, property).FromMaybe(false)) {

			// Delete property from parent global.
			parentGlobal->Delete(context, property);
		}
	}

	void ModuleContext::EnumeratorCallback(const PropertyCallbackInfo<Array>& info)
	{
		Isolate* isolate = info.GetIsolate();
		Local<Context> context = isolate->GetCurrentContext();
		Local<Object> moduleGlobal = context->Global();

		Local<Array> data = info.Data().As<Array>();
		Local<Boolean> handlersEnabled = data->Get(context, DataIndex::kHandlersEnabled).ToLocalChecked().As<Boolean>();
		Local<Object> parentGlobal = data->Get(context, DataIndex::kParentGlobal).ToLocalChecked().As<Object>();

		if (!handlersEnabled->Value()) {
			return;
		}

		// Obtain properties from parent global.
		Local<Array> parentProperties = parentGlobal->GetPropertyNames(context).ToLocalChecked();
		
		// Obtain properties from module global.
		Local<Array> moduleProperties = moduleGlobal->GetPropertyNames(context).ToLocalChecked();

		// Create set to store all property names.
		Local<Set> properties = Set::New(isolate);

		for (int i = 0; i < parentProperties->Length(); i++) {
			Local<String> name = parentProperties->Get(context, i).ToLocalChecked().As<String>();

			properties->Add(context, name);
		}
		for (int i = 0; i < moduleProperties->Length(); i++) {
			Local<String> name = moduleProperties->Get(context, i).ToLocalChecked().As<String>();

			properties->Add(context, name);
		}

		// Return all properties.
		info.GetReturnValue().Set(properties->AsArray());
	}

	void ModuleContext::DefinerCallback(Local<Name> property, const PropertyDescriptor& desc, const PropertyCallbackInfo<Value>& info)
	{
		Isolate* isolate = info.GetIsolate();
		Local<Context> context = isolate->GetCurrentContext();

		Local<Array> data = info.Data().As<Array>();
		Local<Boolean> handlersEnabled = data->Get(context, DataIndex::kHandlersEnabled).ToLocalChecked().As<Boolean>();
		Local<Object> parentGlobal = data->Get(context, DataIndex::kParentGlobal).ToLocalChecked().As<Object>();
		Local<Object> extensions = data->Get(context, DataIndex::kExtensions).ToLocalChecked().As<Object>();

		if (!handlersEnabled->Value()) {
			return;
		}

		Local<Object> builtins = context->GetEmbedderData(EmbedderIndex::kBuiltins).As<Object>();
		if (builtins->HasRealNamedProperty(context, property).FromMaybe(false)) {
			return;
		}

		auto attributes = PropertyAttribute::None;

		if (!extensions->HasRealNamedProperty(context, property).FromMaybe(false)
				&& parentGlobal->GetRealNamedPropertyAttributes(context, property).To(&attributes)) {
			bool readOnly = static_cast<int>(attributes) & static_cast<int>(PropertyAttribute::ReadOnly);

			if (!readOnly) {

				// Property defined on main global, overwrite property.
				parentGlobal->DefineProperty(context, property, const_cast<PropertyDescriptor &>(desc));

				// Do not set on module global.
				info.GetReturnValue().Set(false);
			}
		}
	}

	void ModuleContext::DescriptorCallback(Local<Name> property, const PropertyCallbackInfo<Value>& info)
	{
		Isolate* isolate = info.GetIsolate();
		Local<Context> context = isolate->GetCurrentContext();

		Local<Array> data = info.Data().As<Array>();
		Local<Boolean> handlersEnabled = data->Get(context, DataIndex::kHandlersEnabled).ToLocalChecked().As<Boolean>();
		Local<Object> parentGlobal = data->Get(context, DataIndex::kParentGlobal).ToLocalChecked().As<Object>();
		Local<Object> extensions = data->Get(context, DataIndex::kExtensions).ToLocalChecked().As<Object>();

		if (!handlersEnabled->Value()) {
			return;
		}

		Local<Object> builtins = context->GetEmbedderData(EmbedderIndex::kBuiltins).As<Object>();
		if (builtins->HasRealNamedProperty(context, property).FromMaybe(false)) {
			return;
		}

		if (!extensions->HasRealNamedProperty(context, property).FromMaybe(false)
				&& parentGlobal->HasRealNamedProperty(context, property).FromMaybe(false)) {
			Local<Value> descriptor = parentGlobal->GetOwnPropertyDescriptor(context, property).ToLocalChecked();

			// Property defined on parent global, return descriptor.
			info.GetReturnValue().Set(descriptor);
		}
	}

	void ModuleContext::IndexedGetterCallback(uint32_t index, const PropertyCallbackInfo<Value>& info)
	{
		Isolate* isolate = info.GetIsolate();
		Local<Context> context = isolate->GetCurrentContext();

		ModuleContext::GetterCallback(Uint32ToName(context, index), info);
	}

	void ModuleContext::IndexedSetterCallback(uint32_t index, Local<Value> value, const PropertyCallbackInfo<Value>& info)
	{
		Isolate* isolate = info.GetIsolate();
		Local<Context> context = isolate->GetCurrentContext();

		ModuleContext::SetterCallback(Uint32ToName(context, index), value, info);
	}

	void ModuleContext::IndexedQueryCallback(uint32_t index, const PropertyCallbackInfo<Integer>& info)
	{
		Isolate* isolate = info.GetIsolate();
		Local<Context> context = isolate->GetCurrentContext();

		ModuleContext::QueryCallback(Uint32ToName(context, index), info);
	}

	void ModuleContext::IndexedDeleterCallback(uint32_t index, const PropertyCallbackInfo<Boolean>& info)
	{
		Isolate* isolate = info.GetIsolate();
		Local<Context> context = isolate->GetCurrentContext();

		ModuleContext::DeleterCallback(Uint32ToName(context, index), info);
	}

	void ModuleContext::IndexedEnumeratorCallback(const PropertyCallbackInfo<Array>& info)
	{
		ModuleContext::EnumeratorCallback(info);
	}

	void ModuleContext::IndexedDefinerCallback(uint32_t index, const PropertyDescriptor& desc, const PropertyCallbackInfo<Value>& info)
	{
		Isolate* isolate = info.GetIsolate();
		Local<Context> context = isolate->GetCurrentContext();

		ModuleContext::DefinerCallback(Uint32ToName(context, index), desc, info);
	}

	void ModuleContext::IndexedDescriptorCallback(uint32_t index, const PropertyCallbackInfo<Value>& info)
	{
		Isolate* isolate = info.GetIsolate();
		Local<Context> context = isolate->GetCurrentContext();

		ModuleContext::DescriptorCallback(Uint32ToName(context, index), info);
	}
}
