/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#pragma once

#include "projectstorageexceptions.h"
#include "projectstorageids.h"
#include "projectstoragetypes.h"
#include "sourcepathcachetypes.h"

#include <sqlitealgorithms.h>
#include <sqlitetable.h>
#include <sqlitetransaction.h>

#include <utils/algorithm.h>
#include <utils/optional.h>

#include <tuple>

namespace QmlDesigner {

template<typename Database>
class ProjectStorage
{
public:
    template<int ResultCount>
    using ReadStatement = typename Database::template ReadStatement<ResultCount>;
    template<int ResultCount>
    using ReadWriteStatement = typename Database::template ReadWriteStatement<ResultCount>;
    using WriteStatement = typename Database::WriteStatement;

    ProjectStorage(Database &database, bool isInitialized)
        : database{database}
        , initializer{database, isInitialized}
    {}

    void synchronizeTypes(Storage::Types types, SourceIds sourceIds)
    {
        Sqlite::ImmediateTransaction transaction{database};

        TypeIds updatedTypeIds;
        updatedTypeIds.reserve(types.size());

        for (auto &&type : types) {
            if (!type.sourceId)
                throw TypeHasInvalidSourceId{};

            updatedTypeIds.push_back(declareType(type));
        }

        for (auto &&type : types)
            synchronizeAliasPropertyDeclarationsRemoval(type);

        for (auto &&type : types)
            syncType(type);

        for (auto &&type : types)
            synchronizeAliasPropertyDeclarations(type);

        deleteNotUpdatedTypes(updatedTypeIds, sourceIds);

        transaction.commit();
    }

    void synchronizeImports(Storage::Imports imports)
    {
        Sqlite::ImmediateTransaction transaction{database};

        synchronizeImportsAndUpdatesImportIds(imports);
        synchronizeImportDependencies(createSortedImportDependecies(imports));

        transaction.commit();
    }

    ImportIds fetchImportIds(const Storage::Imports &imports)
    {
        ImportIds importIds;

        Sqlite::DeferredTransaction transaction{database};

        for (auto &&import : imports)
            importIds.push_back(fetchImportId(import));

        transaction.commit();

        return importIds;
    }

    ImportIds fetchImportDependencyIds(ImportIds importIds) const
    {
        return fetchImportDependencyIdsStatement.template valuesWithTransaction<ImportId>(
            16, static_cast<void *>(importIds.data()), static_cast<long long>(importIds.size()));
    }

    PropertyDeclarationId fetchPropertyDeclarationByTypeIdAndName(TypeId typeId,
                                                                  Utils::SmallStringView name)
    {
        return selectPropertyDeclarationIdByTypeIdAndNameStatement
            .template valueWithTransaction<PropertyDeclarationId>(&typeId, name);
    }

    TypeId fetchTypeIdByExportedName(Utils::SmallStringView name) const
    {
        return selectTypeIdByExportedNameStatement.template valueWithTransaction<TypeId>(name);
    }

    TypeId fetchTypeIdByImportIdsAndExportedName(ImportIds importIds, Utils::SmallStringView name) const
    {
        return selectTypeIdByImportIdsAndExportedNameStatement.template valueWithTransaction<TypeId>(
            static_cast<void *>(importIds.data()), static_cast<long long>(importIds.size()), name);
    }

    TypeId fetchTypeIdByName(ImportId importId, Utils::SmallStringView name)
    {
        return selectTypeIdByImportIdAndNameStatement.template valueWithTransaction<TypeId>(&importId,
                                                                                            name);
    }

    Storage::Type fetchTypeByTypeId(TypeId typeId)
    {
        Sqlite::DeferredTransaction transaction{database};

        auto type = selectTypeByTypeIdStatement.template value<Storage::Type>(&typeId);

        type.exportedTypes = fetchExportedTypes(typeId);

        transaction.commit();

        return type;
    }

    Storage::Types fetchTypes()
    {
        Sqlite::DeferredTransaction transaction{database};

        auto types = selectTypesStatement.template values<Storage::Type>(64);

        for (Storage::Type &type : types) {
            type.exportedTypes = fetchExportedTypes(type.typeId);
            type.propertyDeclarations = fetchPropertyDeclarations(type.typeId);
            type.functionDeclarations = fetchFunctionDeclarations(type.typeId);
            type.signalDeclarations = fetchSignalDeclarations(type.typeId);
            type.enumerationDeclarations = fetchEnumerationDeclarations(type.typeId);
        }

        transaction.commit();

        return types;
    }

    bool fetchIsProtype(TypeId type, TypeId prototype)
    {
        return bool(
            selectPrototypeIdStatement.template valueWithTransaction<TypeId>(&type, &prototype));
    }

    auto fetchPrototypes(TypeId type)
    {
        return selectPrototypeIdsStatement.template rangeWithTransaction<TypeId>(&type);
    }

    SourceContextId fetchSourceContextIdUnguarded(Utils::SmallStringView sourceContextPath)
    {
        auto sourceContextId = readSourceContextId(sourceContextPath);

        return sourceContextId ? sourceContextId : writeSourceContextId(sourceContextPath);
    }

    SourceContextId fetchSourceContextId(Utils::SmallStringView sourceContextPath)
    {
        try {
            Sqlite::DeferredTransaction transaction{database};

            auto sourceContextId = fetchSourceContextIdUnguarded(sourceContextPath);

            transaction.commit();

            return sourceContextId;
        } catch (const Sqlite::ConstraintPreventsModification &) {
            return fetchSourceContextId(sourceContextPath);
        }
    }

    Utils::PathString fetchSourceContextPath(SourceContextId sourceContextId) const
    {
        Sqlite::DeferredTransaction transaction{database};

        auto optionalSourceContextPath = selectSourceContextPathFromSourceContextsBySourceContextIdStatement
                                             .template optionalValue<Utils::PathString>(
                                                 &sourceContextId);

        if (!optionalSourceContextPath)
            throw SourceContextIdDoesNotExists();

        transaction.commit();

        return std::move(*optionalSourceContextPath);
    }

    auto fetchAllSourceContexts() const
    {
        return selectAllSourceContextsStatement.template valuesWithTransaction<Cache::SourceContext>(
            128);
    }

    SourceId fetchSourceId(SourceContextId sourceContextId, Utils::SmallStringView sourceName)
    {
        Sqlite::DeferredTransaction transaction{database};

        auto sourceId = fetchSourceIdUnguarded(sourceContextId, sourceName);

        transaction.commit();

        return sourceId;
    }

    auto fetchSourceNameAndSourceContextId(SourceId sourceId) const
    {
        auto value = selectSourceNameAndSourceContextIdFromSourcesBySourceIdStatement
                         .template valueWithTransaction<Cache::SourceNameAndSourceContextId>(&sourceId);

        if (!value.sourceContextId)
            throw SourceIdDoesNotExists();

        return value;
    }

    SourceContextId fetchSourceContextId(SourceId sourceId) const
    {
        auto sourceContextId = selectSourceContextIdFromSourcesBySourceIdStatement
                                   .template valueWithTransaction<SourceContextId>(sourceId.id);

        if (!sourceContextId)
            throw SourceIdDoesNotExists();

        return sourceContextId;
    }

    auto fetchAllSources() const
    {
        return selectAllSourcesStatement.template valuesWithTransaction<Cache::Source>(1024);
    }

    SourceId fetchSourceIdUnguarded(SourceContextId sourceContextId, Utils::SmallStringView sourceName)
    {
        auto sourceId = readSourceId(sourceContextId, sourceName);

        if (sourceId)
            return sourceId;

        return writeSourceId(sourceContextId, sourceName);
    }

    auto fetchAllImports() const
    {
        Storage::Imports imports;
        imports.reserve(128);

        auto callback = [&](Utils::SmallStringView name, int version, int sourceId, long long importId) {
            auto &lastImport = imports.emplace_back(name,
                                                    Storage::VersionNumber{version},
                                                    SourceId{sourceId});

            lastImport.importDependencies = selectImportsForThatDependentOnThisImportIdStatement
                                                .template values<Storage::BasicImport>(6, importId);

            return Sqlite::CallbackControl::Continue;
        };

        selectAllImportsStatement.readCallbackWithTransaction(callback);

        return imports;
    }

private:
    struct ImportDependency
    {
        ImportDependency(ImportId id, ImportId dependencyId)
            : id{id}
            , dependencyId{dependencyId}
        {}

        ImportDependency(long long id, long long dependencyId)
            : id{id}
            , dependencyId{dependencyId}
        {}

        ImportId id;
        ImportId dependencyId;

        friend bool operator<(ImportDependency first, ImportDependency second)
        {
            return std::tie(first.id, first.dependencyId) < std::tie(second.id, second.dependencyId);
        }

        friend bool operator==(ImportDependency first, ImportDependency second)
        {
            return first.id == second.id && first.dependencyId == second.dependencyId;
        }
    };

    void synchronizeImportsAndUpdatesImportIds(Storage::Imports &imports)
    {
        auto compareKey = [](auto &&first, auto &&second) {
            auto nameCompare = Sqlite::compare(first.name, second.name);

            if (nameCompare != 0)
                return nameCompare;

            return first.version.version - second.version.version;
        };

        std::sort(imports.begin(), imports.end(), [&](auto &&first, auto &&second) {
            return compareKey(first, second) < 0;
        });

        auto range = selectAllImportsStatement.template range<Storage::ImportView>();

        auto insert = [&](Storage::Import &import) {
            import.importId = insertImportStatement.template value<ImportId>(import.name,
                                                                             import.version.version,
                                                                             &import.sourceId);
        };

        auto update = [&](const Storage::ImportView &importView, Storage::Import &import) {
            if (importView.sourceId.id != import.sourceId.id)
                updateImportStatement.write(&importView.importId, &import.sourceId);
            import.importId = importView.importId;
        };

        auto remove = [&](const Storage::ImportView &importView) {
            deleteImportStatement.write(&importView.importId);
            deleteTypesForImportId(importView.importId);
        };

        Sqlite::insertUpdateDelete(range, imports, compareKey, insert, update, remove);
    }

    std::vector<ImportDependency> createSortedImportDependecies(const Storage::Imports &imports) const
    {
        std::vector<ImportDependency> importDependecies;
        importDependecies.reserve(imports.size() * 5);

        for (const Storage::Import &import : imports) {
            for (const Storage::BasicImport &importDependency : import.importDependencies) {
                auto importIdForDependency = fetchImportId(importDependency);

                if (!importIdForDependency)
                    throw ImportDoesNotExists{};

                importDependecies.emplace_back(import.importId, importIdForDependency);
            }
        }

        std::sort(importDependecies.begin(), importDependecies.end());
        importDependecies.erase(std::unique(importDependecies.begin(), importDependecies.end()),
                                importDependecies.end());

        return importDependecies;
    }

    void synchronizeImportDependencies(const std::vector<ImportDependency> &importDependecies)
    {
        auto compareKey = [](ImportDependency first, ImportDependency second) {
            auto idCompare = first.id.id - second.id.id;

            if (idCompare != 0)
                return idCompare;

            return first.dependencyId.id - second.dependencyId.id;
        };

        auto range = selectAllImportDependenciesStatement.template range<ImportDependency>();

        auto insert = [&](ImportDependency dependency) {
            insertImportDependencyStatement.write(&dependency.id, &dependency.dependencyId);
        };

        auto update = [](ImportDependency, ImportDependency) {};

        auto remove = [&](ImportDependency dependency) {
            deleteImportDependencyStatement.write(&dependency.id, &dependency.dependencyId);
        };

        Sqlite::insertUpdateDelete(range, importDependecies, compareKey, insert, update, remove);
    }

    ImportId fetchImportId(const Storage::BasicImport &import) const
    {
        if (import.version) {
            return selectImportIdByNameAndVersionStatement
                .template value<ImportId>(import.name, import.version.version);
        }

        return selectImportIdByNameStatement.template value<ImportId>(import.name);
    }

    void deleteType(TypeId typeId)
    {
        deleteExportTypesByTypeIdStatement.write(&typeId);
        deleteEnumerationDeclarationByTypeIdStatement.write(&typeId);
        deletePropertyDeclarationByTypeIdStatement.write(&typeId);
        deleteFunctionDeclarationByTypeIdStatement.write(&typeId);
        deleteSignalDeclarationByTypeIdStatement.write(&typeId);
        deleteTypeStatement.write(&typeId);
    }

    void deleteNotUpdatedTypes(const TypeIds &updatedTypeIds, const SourceIds &sourceIds)
    {
        auto updatedTypeIdValues = Utils::transform<std::vector>(updatedTypeIds, [](TypeId typeId) {
            return &typeId;
        });

        auto sourceIdValues = Utils::transform<std::vector>(sourceIds, [](SourceId sourceId) {
            return &sourceId;
        });

        auto callback = [&](long long typeId) {
            deleteType(TypeId{typeId});
            return Sqlite::CallbackControl::Continue;
        };

        selectNotUpdatedTypesInSourcesStatement.readCallback(callback,
                                                             Utils::span(sourceIdValues),
                                                             Utils::span(updatedTypeIdValues));
    }

    void deleteTypesForImportId(ImportId importId)
    {
        auto callback = [&](long long typeId) {
            deleteType(TypeId{typeId});
            return Sqlite::CallbackControl::Continue;
        };

        selectTypeIdsForImportIdStatement.readCallback(callback, &importId);
    }

    void upsertExportedType(ImportId importId, Utils::SmallStringView name, TypeId typeId)
    {
        upsertExportedTypesStatement.write(&importId, name, &typeId);
    }

    void synchronizePropertyDeclarations(TypeId typeId,
                                         Storage::PropertyDeclarations &propertyDeclarations,
                                         ImportIds &importIds)
    {
        std::sort(propertyDeclarations.begin(),
                  propertyDeclarations.end(),
                  [](auto &&first, auto &&second) {
                      return Sqlite::compare(first.name, second.name) < 0;
                  });

        auto range = selectPropertyDeclarationsForTypeIdStatement
                         .template range<Storage::PropertyDeclarationView>(&typeId);

        auto compareKey = [](const Storage::PropertyDeclarationView &view,
                             const Storage::PropertyDeclaration &value) {
            return Sqlite::compare(view.name, value.name);
        };

        auto insert = [&](const Storage::PropertyDeclaration &value) {
            auto propertyTypeId = fetchTypeIdByNameUngarded(value.typeName, importIds);

            insertPropertyDeclarationStatement.write(&typeId,
                                                     value.name,
                                                     &propertyTypeId,
                                                     static_cast<int>(value.traits));
        };

        auto update = [&](const Storage::PropertyDeclarationView &view,
                          const Storage::PropertyDeclaration &value) {
            auto propertyTypeId = fetchTypeIdByNameUngarded(value.typeName, importIds);

            if (view.traits == value.traits && propertyTypeId == view.typeId)
                return;

            updatePropertyDeclarationStatement.write(&view.id,
                                                     &propertyTypeId,
                                                     static_cast<int>(value.traits));
        };

        auto remove = [&](const Storage::PropertyDeclarationView &view) {
            deletePropertyDeclarationStatement.write(&view.id);
        };

        Sqlite::insertUpdateDelete(range, propertyDeclarations, compareKey, insert, update, remove);
    }

    void synchronizeAliasPropertyDeclarationsRemoval(Storage::Type &type)
    {
        auto &aliasDeclarations = type.aliasDeclarations;
        TypeId typeId = type.typeId;

        std::sort(aliasDeclarations.begin(), aliasDeclarations.end(), [](auto &&first, auto &&second) {
            return Sqlite::compare(first.name, second.name) < 0;
        });

        auto range = selectPropertyDeclarationsWithAliasForTypeIdStatement
                         .template range<Storage::AliasPropertyDeclarationView>(&typeId);

        auto compareKey = [](const Storage::AliasPropertyDeclarationView &view,
                             const Storage::AliasPropertyDeclaration &value) {
            return Sqlite::compare(view.name, value.name);
        };

        auto insert = [&](const Storage::AliasPropertyDeclaration &) {};

        auto update = [&](const Storage::AliasPropertyDeclarationView &,
                          const Storage::AliasPropertyDeclaration &) {};

        auto remove = [&](const Storage::AliasPropertyDeclarationView &view) {
            deletePropertyDeclarationStatement.write(&view.id);
        };

        Sqlite::insertUpdateDelete(range, aliasDeclarations, compareKey, insert, update, remove);
    }

    void synchronizeAliasPropertyDeclarations(Storage::Type &type)
    {
        auto &aliasDeclarations = type.aliasDeclarations;
        TypeId typeId = type.typeId;
        ImportIds &importIds = type.importIds;

        std::sort(aliasDeclarations.begin(), aliasDeclarations.end(), [](auto &&first, auto &&second) {
            return Sqlite::compare(first.name, second.name) < 0;
        });

        auto range = selectPropertyDeclarationsWithAliasForTypeIdStatement
                         .template range<Storage::AliasPropertyDeclarationView>(&typeId);

        auto compareKey = [](const Storage::AliasPropertyDeclarationView &view,
                             const Storage::AliasPropertyDeclaration &value) {
            return Sqlite::compare(view.name, value.name);
        };

        auto insert = [&](const Storage::AliasPropertyDeclaration &value) {
            auto [aliasTypeId, aliasId, propertyTraits] = fetchPropertyDeclarationByTypeNameAndName(
                value.aliasTypeName, value.aliasPropertyName, importIds);

            insertPropertyDeclarationWithAliasStatement.write(&typeId,
                                                              value.name,
                                                              &aliasTypeId,
                                                              propertyTraits,
                                                              &aliasId);
        };

        auto update = [&](const Storage::AliasPropertyDeclarationView &view,
                          const Storage::AliasPropertyDeclaration &value) {
            auto [aliasTypeId, aliasId, propertyTraits] = fetchPropertyDeclarationByTypeNameAndName(
                value.aliasTypeName, value.aliasPropertyName, importIds);

            if (view.aliasId == aliasId)
                return;

            updatePropertyDeclarationWithAliasStatement.write(&view.id,
                                                              &aliasTypeId,
                                                              propertyTraits,
                                                              &aliasId);
        };

        auto remove = [&](const Storage::AliasPropertyDeclarationView &) {};

        Sqlite::insertUpdateDelete(range, aliasDeclarations, compareKey, insert, update, remove);
    }

    Utils::PathString createJson(const Storage::ParameterDeclarations &parameters)
    {
        Utils::PathString json;
        json.append("[");

        Utils::SmallStringView comma{""};

        for (const auto &parameter : parameters) {
            json.append(comma);
            comma = ",";
            json.append("{\"n\":\"");
            json.append(parameter.name);
            json.append("\",\"tn\":\"");
            json.append(parameter.typeName);
            if (parameter.traits == Storage::PropertyDeclarationTraits::Non) {
                json.append("\"}");
            } else {
                json.append("\",\"tr\":");
                json.append(Utils::SmallString::number(static_cast<int>(parameter.traits)));
                json.append("}");
            }
        }

        json.append("]");

        return json;
    }

    void synchronizeFunctionDeclarations(TypeId typeId,
                                         Storage::FunctionDeclarations &functionsDeclarations)
    {
        std::sort(functionsDeclarations.begin(),
                  functionsDeclarations.end(),
                  [](auto &&first, auto &&second) {
                      return Sqlite::compare(first.name, second.name) < 0;
                  });

        auto range = selectFunctionDeclarationsForTypeIdStatement
                         .template range<Storage::FunctionDeclarationView>(&typeId);

        auto compareKey = [](const Storage::FunctionDeclarationView &view,
                             const Storage::FunctionDeclaration &value) {
            return Sqlite::compare(view.name, value.name);
        };

        auto insert = [&](const Storage::FunctionDeclaration &value) {
            Utils::PathString signature{createJson(value.parameters)};

            insertFunctionDeclarationStatement.write(&typeId, value.name, value.returnTypeName, signature);
        };

        auto update = [&](const Storage::FunctionDeclarationView &view,
                          const Storage::FunctionDeclaration &value) {
            Utils::PathString signature{createJson(value.parameters)};

            if (value.returnTypeName == view.returnTypeName && signature == view.signature)
                return;

            updateFunctionDeclarationStatement.write(&view.id, value.returnTypeName, signature);
        };

        auto remove = [&](const Storage::FunctionDeclarationView &view) {
            deleteFunctionDeclarationStatement.write(&view.id);
        };

        Sqlite::insertUpdateDelete(range, functionsDeclarations, compareKey, insert, update, remove);
    }

    void synchronizeSignalDeclarations(TypeId typeId, Storage::SignalDeclarations &signalDeclarations)
    {
        std::sort(signalDeclarations.begin(), signalDeclarations.end(), [](auto &&first, auto &&second) {
            return Sqlite::compare(first.name, second.name) < 0;
        });

        auto range = selectSignalDeclarationsForTypeIdStatement
                         .template range<Storage::SignalDeclarationView>(&typeId);

        auto compareKey = [](const Storage::SignalDeclarationView &view,
                             const Storage::SignalDeclaration &value) {
            return Sqlite::compare(view.name, value.name);
        };

        auto insert = [&](const Storage::SignalDeclaration &value) {
            Utils::PathString signature{createJson(value.parameters)};

            insertSignalDeclarationStatement.write(&typeId, value.name, signature);
        };

        auto update = [&](const Storage::SignalDeclarationView &view,
                          const Storage::SignalDeclaration &value) {
            Utils::PathString signature{createJson(value.parameters)};

            if (signature == view.signature)
                return;

            updateSignalDeclarationStatement.write(&view.id, signature);
        };

        auto remove = [&](const Storage::SignalDeclarationView &view) {
            deleteSignalDeclarationStatement.write(&view.id);
        };

        Sqlite::insertUpdateDelete(range, signalDeclarations, compareKey, insert, update, remove);
    }

    Utils::PathString createJson(const Storage::EnumeratorDeclarations &enumeratorDeclarations)
    {
        Utils::PathString json;
        json.append("{");

        Utils::SmallStringView comma{"\""};

        for (const auto &enumerator : enumeratorDeclarations) {
            json.append(comma);
            comma = ",\"";
            json.append(enumerator.name);
            if (enumerator.hasValue) {
                json.append("\":\"");
                json.append(Utils::SmallString::number(enumerator.value));
                json.append("\"");
            } else {
                json.append("\":null");
            }
        }

        json.append("}");

        return json;
    }

    void synchronizeEnumerationDeclarations(TypeId typeId,
                                            Storage::EnumerationDeclarations &enumerationDeclarations)
    {
        std::sort(enumerationDeclarations.begin(),
                  enumerationDeclarations.end(),
                  [](auto &&first, auto &&second) {
                      return Sqlite::compare(first.name, second.name) < 0;
                  });

        auto range = selectEnumerationDeclarationsForTypeIdStatement
                         .template range<Storage::EnumerationDeclarationView>(&typeId);

        auto compareKey = [](const Storage::EnumerationDeclarationView &view,
                             const Storage::EnumerationDeclaration &value) {
            return Sqlite::compare(view.name, value.name);
        };

        auto insert = [&](const Storage::EnumerationDeclaration &value) {
            Utils::PathString signature{createJson(value.enumeratorDeclarations)};

            insertEnumerationDeclarationStatement.write(&typeId, value.name, signature);
        };

        auto update = [&](const Storage::EnumerationDeclarationView &view,
                          const Storage::EnumerationDeclaration &value) {
            Utils::PathString enumeratorDeclarations{createJson(value.enumeratorDeclarations)};

            if (enumeratorDeclarations == view.enumeratorDeclarations)
                return;

            updateEnumerationDeclarationStatement.write(&view.id, enumeratorDeclarations);
        };

        auto remove = [&](const Storage::EnumerationDeclarationView &view) {
            deleteEnumerationDeclarationStatement.write(&view.id);
        };

        Sqlite::insertUpdateDelete(range, enumerationDeclarations, compareKey, insert, update, remove);
    }

    TypeId declareType(Storage::Type &type)
    {
        type.typeId = upsertTypeStatement.template value<TypeId>(&type.importId,
                                                                 type.typeName,
                                                                 static_cast<int>(type.accessSemantics),
                                                                 &type.sourceId);

        for (const auto &exportedType : type.exportedTypes)
            upsertExportedType(type.importId, exportedType.name, type.typeId);

        return type.typeId;
    }

    void syncType(Storage::Type &type)
    {
        auto typeId = type.typeId;

        auto prototypeId = fetchTypeIdByNameUngarded(type.prototype, type.importIds);

        updatePrototypeStatement.write(&typeId, &prototypeId);

        synchronizePropertyDeclarations(typeId, type.propertyDeclarations, type.importIds);
        synchronizeFunctionDeclarations(typeId, type.functionDeclarations);
        synchronizeSignalDeclarations(typeId, type.signalDeclarations);
        synchronizeEnumerationDeclarations(typeId, type.enumerationDeclarations);
    }

    TypeId fetchTypeIdByNameUngarded(const Storage::TypeName &name, ImportIds &importIds)
    {
        if (Utils::visit([](auto &&type) -> bool { return type.name.isEmpty(); }, name))
            return TypeId{};

        struct Inspect
        {
            TypeId operator()(const Storage::NativeType &nativeType)
            {
                return storage.selectTypeIdByImportIdsAndNameStatement
                    .template value<TypeId>(static_cast<void *>(importIds.data()),
                                            static_cast<long long>(importIds.size()),
                                            nativeType.name);
            }

            TypeId operator()(const Storage::ExportedType &exportedType)
            {
                return storage.selectTypeIdByImportIdsAndExportedNameStatement
                    .template value<TypeId>(static_cast<void *>(importIds.data()),
                                            static_cast<long long>(importIds.size()),
                                            exportedType.name);
            }

            TypeId operator()(const Storage::ExplicitExportedType &exportedType)
            {
                return storage.selectTypeIdByImportIdAndExportedNameStatement
                    .template value<TypeId>(&exportedType.importId, exportedType.name);
            }

            ProjectStorage &storage;
            ImportIds &importIds;
        };

        auto typeId = Utils::visit(Inspect{*this, importIds}, name);

        if (typeId)
            return typeId;

        throw TypeNameDoesNotExists{};
    }

    using PropertyDeclarationViewTuple = std::tuple<TypeId, PropertyDeclarationId, long long>;

    PropertyDeclarationViewTuple fetchPropertyDeclarationByTypeNameAndName(
        const Storage::TypeName &typeName, Utils::SmallStringView name, ImportIds &importIds)
    {
        TypeId typeId = fetchTypeIdByNameUngarded(typeName, importIds);

        auto propertyDeclaration = selectPropertyDeclarationByTypeIdAndNameStatement
                                       .template value<PropertyDeclarationViewTuple>(&typeId, name);

        if (auto id = std::get<PropertyDeclarationId>(propertyDeclaration); id)
            return propertyDeclaration;

        throw PropertyNameDoesNotExists{};
    }

    SourceContextId readSourceContextId(Utils::SmallStringView sourceContextPath)
    {
        return selectSourceContextIdFromSourceContextsBySourceContextPathStatement
            .template value<SourceContextId>(sourceContextPath);
    }

    SourceContextId writeSourceContextId(Utils::SmallStringView sourceContextPath)
    {
        insertIntoSourceContextsStatement.write(sourceContextPath);

        return SourceContextId(database.lastInsertedRowId());
    }

    SourceId writeSourceId(SourceContextId sourceContextId, Utils::SmallStringView sourceName)
    {
        insertIntoSourcesStatement.write(&sourceContextId, sourceName);

        return SourceId(database.lastInsertedRowId());
    }

    SourceId readSourceId(SourceContextId sourceContextId, Utils::SmallStringView sourceName)
    {
        return selectSourceIdFromSourcesBySourceContextIdAndSourceNameStatement
            .template value<SourceId>(&sourceContextId, sourceName);
    }

    auto fetchExportedTypes(TypeId typeId)
    {
        return selectExportedTypesByTypeIdStatement.template values<Storage::ExportedType>(12,
                                                                                           &typeId);
    }

    auto fetchPropertyDeclarations(TypeId typeId)
    {
        return selectPropertyDeclarationsByTypeIdStatement
            .template values<Storage::PropertyDeclaration>(24, &typeId);
    }

    auto fetchFunctionDeclarations(TypeId typeId)
    {
        Storage::FunctionDeclarations functionDeclarations;

        auto callback = [&](Utils::SmallStringView name,
                            Utils::SmallStringView returnType,
                            long long functionDeclarationId) {
            auto &functionDeclaration = functionDeclarations.emplace_back(name, returnType);
            functionDeclaration.parameters = selectFunctionParameterDeclarationsStatement
                                                 .template values<Storage::ParameterDeclaration>(
                                                     8, functionDeclarationId);

            return Sqlite::CallbackControl::Continue;
        };

        selectFunctionDeclarationsForTypeIdWithoutSignatureStatement.readCallback(callback, &typeId);

        return functionDeclarations;
    }

    auto fetchSignalDeclarations(TypeId typeId)
    {
        Storage::SignalDeclarations signalDeclarations;

        auto callback = [&](Utils::SmallStringView name, long long signalDeclarationId) {
            auto &signalDeclaration = signalDeclarations.emplace_back(name);
            signalDeclaration.parameters = selectSignalParameterDeclarationsStatement
                                               .template values<Storage::ParameterDeclaration>(
                                                   8, signalDeclarationId);

            return Sqlite::CallbackControl::Continue;
        };

        selectSignalDeclarationsForTypeIdWithoutSignatureStatement.readCallback(callback, &typeId);

        return signalDeclarations;
    }

    auto fetchEnumerationDeclarations(TypeId typeId)
    {
        Storage::EnumerationDeclarations enumerationDeclarations;

        auto callback = [&](Utils::SmallStringView name, long long enumerationDeclarationId) {
            enumerationDeclarations.emplace_back(
                name,
                selectEnumeratorDeclarationStatement
                    .template values<Storage::EnumeratorDeclaration>(8, enumerationDeclarationId));

            return Sqlite::CallbackControl::Continue;
        };

        selectEnumerationDeclarationsForTypeIdWithoutEnumeratorDeclarationsStatement
            .readCallback(callback, &typeId);

        return enumerationDeclarations;
    }

    class Initializer
    {
    public:
        Initializer(Database &database, bool isInitialized)
        {
            if (!isInitialized) {
                Sqlite::ExclusiveTransaction transaction{database};

                createImportsTable(database);
                createImportDependeciesTable(database);
                createSourceContextsTable(database);
                createSourcesTable(database);
                createTypesAndePropertyDeclarationsTables(database);
                createExportedTypesTable(database);
                createEnumerationsTable(database);
                createFunctionsTable(database);
                createSignalsTable(database);

                transaction.commit();

                database.walCheckpointFull();
            }
        }

        void createSourceContextsTable(Database &database)
        {
            Sqlite::Table table;
            table.setUseIfNotExists(true);
            table.setName("sourceContexts");
            table.addColumn("sourceContextId", Sqlite::ColumnType::Integer, {Sqlite::PrimaryKey{}});
            const Sqlite::Column &sourceContextPathColumn = table.addColumn("sourceContextPath");

            table.addUniqueIndex({sourceContextPathColumn});

            table.initialize(database);
        }

        void createSourcesTable(Database &database)
        {
            Sqlite::Table table;
            table.setUseIfNotExists(true);
            table.setName("sources");
            table.addColumn("sourceId", Sqlite::ColumnType::Integer, {Sqlite::PrimaryKey{}});
            const Sqlite::Column &sourceContextIdColumn = table.addColumn(
                "sourceContextId",
                Sqlite::ColumnType::Integer,
                {Sqlite::NotNull{},
                 Sqlite::ForeignKey{"sourceContexts",
                                    "sourceContextId",
                                    Sqlite::ForeignKeyAction::NoAction,
                                    Sqlite::ForeignKeyAction::Cascade}});
            const Sqlite::Column &sourceNameColumn = table.addColumn("sourceName");
            table.addUniqueIndex({sourceContextIdColumn, sourceNameColumn});

            table.initialize(database);
        }

        void createTypesAndePropertyDeclarationsTables(Database &database)
        {
            Sqlite::Table typesTable;
            typesTable.setUseIfNotExists(true);
            typesTable.setName("types");
            typesTable.addColumn("typeId", Sqlite::ColumnType::Integer, {Sqlite::PrimaryKey{}});
            auto &importIdColumn = typesTable.addColumn("importId");
            auto &typesNameColumn = typesTable.addColumn("name");
            typesTable.addColumn("accessSemantics");
            typesTable.addColumn("sourceId");
            typesTable.addForeignKeyColumn("prototypeId",
                                           typesTable,
                                           Sqlite::ForeignKeyAction::NoAction,
                                           Sqlite::ForeignKeyAction::Restrict);

            typesTable.addUniqueIndex({importIdColumn, typesNameColumn});

            typesTable.initialize(database);

            {
                Sqlite::Table propertyDeclarationTable;
                propertyDeclarationTable.setUseIfNotExists(true);
                propertyDeclarationTable.setName("propertyDeclarations");
                propertyDeclarationTable.addColumn("propertyDeclarationId",
                                                   Sqlite::ColumnType::Integer,
                                                   {Sqlite::PrimaryKey{}});
                auto &typeIdColumn = propertyDeclarationTable.addColumn("typeId");
                auto &nameColumn = propertyDeclarationTable.addColumn("name");
                propertyDeclarationTable.addForeignKeyColumn("propertyTypeId",
                                                             typesTable,
                                                             Sqlite::ForeignKeyAction::NoAction,
                                                             Sqlite::ForeignKeyAction::Restrict);
                propertyDeclarationTable.addColumn("propertyTraits");
                auto &aliasPropertyDeclarationIdColumn = propertyDeclarationTable.addForeignKeyColumn(
                    "aliasPropertyDeclarationId",
                    propertyDeclarationTable,
                    Sqlite::ForeignKeyAction::NoAction,
                    Sqlite::ForeignKeyAction::Restrict);

                propertyDeclarationTable.addUniqueIndex({typeIdColumn, nameColumn});
                propertyDeclarationTable.addIndex({aliasPropertyDeclarationIdColumn},
                                                  "aliasPropertyDeclarationId IS NOT NULL");

                propertyDeclarationTable.initialize(database);
            }
        }

        void createExportedTypesTable(Database &database)
        {
            Sqlite::Table table;
            table.setUseIfNotExists(true);
            table.setUseWithoutRowId(true);
            table.setName("exportedTypes");
            auto &importIdColumn = table.addColumn("importId");
            auto &nameColumn = table.addColumn("name");
            table.addColumn("typeId");

            table.addPrimaryKeyContraint({importIdColumn, nameColumn});

            table.initialize(database);
        }

        void createEnumerationsTable(Database &database)
        {
            Sqlite::Table table;
            table.setUseIfNotExists(true);
            table.setName("enumerationDeclarations");
            table.addColumn("enumerationDeclarationId",
                            Sqlite::ColumnType::Integer,
                            {Sqlite::PrimaryKey{}});
            auto &typeIdColumn = table.addColumn("typeId");
            auto &nameColumn = table.addColumn("name");
            table.addColumn("enumeratorDeclarations");

            table.addUniqueIndex({typeIdColumn, nameColumn});

            table.initialize(database);
        }

        void createFunctionsTable(Database &database)
        {
            Sqlite::Table table;
            table.setUseIfNotExists(true);
            table.setName("functionDeclarations");
            table.addColumn("functionDeclarationId",
                            Sqlite::ColumnType::Integer,
                            {Sqlite::PrimaryKey{}});
            auto &typeIdColumn = table.addColumn("typeId");
            auto &nameColumn = table.addColumn("name");
            table.addColumn("signature");
            table.addColumn("returnTypeName");

            table.addUniqueIndex({typeIdColumn, nameColumn});

            table.initialize(database);
        }

        void createSignalsTable(Database &database)
        {
            Sqlite::Table table;
            table.setUseIfNotExists(true);
            table.setName("signalDeclarations");
            table.addColumn("signalDeclarationId", Sqlite::ColumnType::Integer, {Sqlite::PrimaryKey{}});
            auto &typeIdColumn = table.addColumn("typeId");
            auto &nameColumn = table.addColumn("name");
            table.addColumn("signature");

            table.addUniqueIndex({typeIdColumn, nameColumn});

            table.initialize(database);
        }

        void createImportsTable(Database &database)
        {
            Sqlite::Table table;
            table.setUseIfNotExists(true);
            table.setName("imports");
            table.addColumn("importId", Sqlite::ColumnType::Integer, {Sqlite::PrimaryKey{}});
            auto &nameColumn = table.addColumn("name");
            auto &versionColumn = table.addColumn("version");
            table.addColumn("sourceId");

            table.addUniqueIndex({nameColumn, versionColumn});

            table.initialize(database);
        }

        void createImportDependeciesTable(Database &database)
        {
            Sqlite::Table table;
            table.setUseIfNotExists(true);
            table.setUseWithoutRowId(true);
            table.setName("importDependencies");
            auto &importIdColumn = table.addColumn("importId");
            auto &parentImportIdColumn = table.addColumn("parentImportId");

            table.addPrimaryKeyContraint({importIdColumn, parentImportIdColumn});

            table.initialize(database);
        }
    };

public:
    Database &database;
    Initializer initializer;
    ReadWriteStatement<1> upsertTypeStatement{
        "INSERT INTO types(importId, name,  accessSemantics, sourceId) VALUES(?1, ?2, "
        "?3, nullif(?4, -1)) ON "
        "CONFLICT DO UPDATE SET prototypeId=excluded.prototypeId, "
        "accessSemantics=excluded.accessSemantics, sourceId=excluded.sourceId RETURNING typeId",
        database};
    WriteStatement updatePrototypeStatement{
        "UPDATE types SET prototypeId=nullif(?2, -1) WHERE typeId=?1", database};
    mutable ReadStatement<1> selectTypeIdByExportedNameStatement{
        "SELECT typeId FROM exportedTypes WHERE name=?1", database};
    mutable ReadStatement<1> selectPrototypeIdStatement{
        "WITH RECURSIVE "
        "  typeSelection(typeId) AS ("
        "      VALUES(?1) "
        "    UNION ALL "
        "      SELECT prototypeId FROM types JOIN typeSelection USING(typeId)) "
        "SELECT typeId FROM typeSelection WHERE typeId=?2 LIMIT 1",
        database};
    mutable ReadStatement<1> selectPropertyDeclarationIdByTypeIdAndNameStatement{
        "WITH RECURSIVE "
        "  typeSelection(typeId) AS ("
        "      VALUES(?1) "
        "    UNION ALL "
        "      SELECT prototypeId FROM types JOIN typeSelection USING(typeId)) "
        "SELECT propertyDeclarationId FROM propertyDeclarations JOIN typeSelection USING(typeId) "
        "  WHERE name=?2 LIMIT 1",
        database};
    mutable ReadStatement<3> selectPropertyDeclarationByTypeIdAndNameStatement{
        "WITH RECURSIVE "
        "  typeSelection(typeId) AS ("
        "      VALUES(?1) "
        "    UNION ALL "
        "      SELECT prototypeId FROM types JOIN typeSelection USING(typeId)) "
        "SELECT propertyTypeId, propertyDeclarationId, propertyTraits FROM propertyDeclarations "
        "    JOIN typeSelection USING(typeId) "
        "  WHERE name=?2 LIMIT 1",
        database};
    WriteStatement upsertExportedTypesStatement{"INSERT INTO exportedTypes(importId, name, typeId) "
                                                "VALUES(?1, ?2, ?3) ON CONFLICT DO NOTHING",
                                                database};
    mutable ReadStatement<1> selectPrototypeIdsStatement{
        "WITH RECURSIVE "
        "  typeSelection(typeId) AS ("
        "      VALUES(?1) "
        "    UNION ALL "
        "      SELECT prototypeId FROM types JOIN typeSelection USING(typeId)) "
        "SELECT typeId FROM typeSelection",
        database};
    mutable ReadStatement<1> selectSourceContextIdFromSourceContextsBySourceContextPathStatement{
        "SELECT sourceContextId FROM sourceContexts WHERE sourceContextPath = ?", database};
    mutable ReadStatement<1> selectSourceContextPathFromSourceContextsBySourceContextIdStatement{
        "SELECT sourceContextPath FROM sourceContexts WHERE sourceContextId = ?", database};
    mutable ReadStatement<2> selectAllSourceContextsStatement{
        "SELECT sourceContextPath, sourceContextId FROM sourceContexts", database};
    WriteStatement insertIntoSourceContextsStatement{
        "INSERT INTO sourceContexts(sourceContextPath) VALUES (?)", database};
    mutable ReadStatement<1> selectSourceIdFromSourcesBySourceContextIdAndSourceNameStatement{
        "SELECT sourceId FROM sources WHERE sourceContextId = ? AND sourceName = ?", database};
    mutable ReadStatement<2> selectSourceNameAndSourceContextIdFromSourcesBySourceIdStatement{
        "SELECT sourceName, sourceContextId FROM sources WHERE sourceId = ?", database};
    mutable ReadStatement<1> selectSourceContextIdFromSourcesBySourceIdStatement{
        "SELECT sourceContextId FROM sources WHERE sourceId = ?", database};
    WriteStatement insertIntoSourcesStatement{
        "INSERT INTO sources(sourceContextId, sourceName) VALUES (?,?)", database};
    mutable ReadStatement<3> selectAllSourcesStatement{
        "SELECT sourceName, sourceContextId, sourceId  FROM sources", database};
    mutable ReadStatement<1> selectTypeIdByImportIdsAndNameStatement{
        "SELECT typeId FROM types WHERE importId IN carray(?1, ?2, 'int64') AND name=?3", database};
    mutable ReadStatement<5> selectTypeByTypeIdStatement{
        "SELECT importId, name, (SELECT name FROM types WHERE typeId=outerTypes.prototypeId), "
        "accessSemantics, ifnull(sourceId, -1) FROM types AS outerTypes WHERE typeId=?",
        database};
    mutable ReadStatement<1> selectExportedTypesByTypeIdStatement{
        "SELECT name FROM exportedTypes WHERE typeId=?", database};
    mutable ReadStatement<6> selectTypesStatement{
        "SELECT importId, name, typeId, (SELECT name FROM types WHERE "
        "typeId=outerTypes.prototypeId), accessSemantics, ifnull(sourceId, -1) FROM types AS "
        "outerTypes",
        database};
    ReadStatement<1> selectNotUpdatedTypesInSourcesStatement{
        "SELECT typeId FROM types WHERE (sourceId IN carray(?1) AND typeId NOT IN carray(?2))",
        database};
    WriteStatement deleteExportTypesByTypeIdStatement{"DELETE FROM exportedTypes WHERE typeId=?",
                                                      database};
    WriteStatement deleteEnumerationDeclarationByTypeIdStatement{
        "DELETE FROM enumerationDeclarations WHERE typeId=?", database};
    WriteStatement deletePropertyDeclarationByTypeIdStatement{
        "DELETE FROM propertyDeclarations WHERE typeId=?", database};
    WriteStatement deleteFunctionDeclarationByTypeIdStatement{
        "DELETE FROM functionDeclarations WHERE typeId=?", database};
    WriteStatement deleteSignalDeclarationByTypeIdStatement{
        "DELETE FROM signalDeclarations WHERE typeId=?", database};
    WriteStatement deleteTypeStatement{"DELETE FROM types  WHERE typeId=?", database};
    mutable ReadStatement<3> selectPropertyDeclarationsByTypeIdStatement{
        "SELECT name, (SELECT name FROM types WHERE typeId=propertyDeclarations.propertyTypeId),"
        "propertyTraits FROM propertyDeclarations WHERE typeId=?",
        database};
    ReadStatement<4> selectPropertyDeclarationsForTypeIdStatement{
        "SELECT name, propertyTraits, propertyTypeId, propertyDeclarationId FROM "
        "propertyDeclarations WHERE typeId=? AND aliasPropertyDeclarationId IS NULL ORDER BY "
        "name",
        database};
    WriteStatement insertPropertyDeclarationStatement{
        "INSERT INTO propertyDeclarations(typeId, name, propertyTypeId, propertyTraits) "
        "VALUES(?1, ?2, ?3, ?4)",
        database};
    WriteStatement updatePropertyDeclarationStatement{
        "UPDATE propertyDeclarations SET propertyTypeId=?2, propertyTraits=?3 WHERE "
        "propertyDeclarationId=?1 OR aliasPropertyDeclarationId=?1",
        database};
    WriteStatement deletePropertyDeclarationStatement{
        "DELETE FROM propertyDeclarations WHERE propertyDeclarationId=?", database};
    ReadStatement<3> selectPropertyDeclarationsWithAliasForTypeIdStatement{
        "SELECT name, propertyDeclarationId, aliasPropertyDeclarationId FROM propertyDeclarations "
        "WHERE typeId=? AND aliasPropertyDeclarationId IS NOT NULL ORDER BY name",
        database};
    WriteStatement insertPropertyDeclarationWithAliasStatement{
        "INSERT INTO propertyDeclarations(typeId, name, propertyTypeId, propertyTraits, "
        "aliasPropertyDeclarationId) VALUES(?1, ?2, ?3, ?4, ?5) ",
        database};
    WriteStatement updatePropertyDeclarationWithAliasStatement{
        "UPDATE propertyDeclarations SET propertyTypeId=?2, propertyTraits=?3, "
        "aliasPropertyDeclarationId=?4 WHERE propertyDeclarationId=?1",
        database};
    mutable ReadStatement<4> selectFunctionDeclarationsForTypeIdStatement{
        "SELECT name, returnTypeName, signature, functionDeclarationId FROM "
        "functionDeclarations WHERE typeId=? ORDER BY name",
        database};
    mutable ReadStatement<3> selectFunctionDeclarationsForTypeIdWithoutSignatureStatement{
        "SELECT name, returnTypeName, functionDeclarationId FROM "
        "functionDeclarations WHERE typeId=? ORDER BY name",
        database};
    mutable ReadStatement<3> selectFunctionParameterDeclarationsStatement{
        "SELECT json_extract(json_each.value, '$.n'), json_extract(json_each.value, '$.tn'), "
        "json_extract(json_each.value, '$.tr') FROM functionDeclarations, "
        "json_each(functionDeclarations.signature) WHERE functionDeclarationId=?",
        database};
    WriteStatement insertFunctionDeclarationStatement{
        "INSERT INTO functionDeclarations(typeId, name, returnTypeName, signature) VALUES(?1, ?2, "
        "?3, ?4)",
        database};
    WriteStatement updateFunctionDeclarationStatement{
        "UPDATE functionDeclarations SET returnTypeName=?2, signature=?3 WHERE "
        "functionDeclarationId=?1",
        database};
    WriteStatement deleteFunctionDeclarationStatement{
        "DELETE FROM functionDeclarations WHERE functionDeclarationId=?", database};
    mutable ReadStatement<3> selectSignalDeclarationsForTypeIdStatement{
        "SELECT name, signature, signalDeclarationId FROM signalDeclarations WHERE typeId=? ORDER "
        "BY name",
        database};
    mutable ReadStatement<2> selectSignalDeclarationsForTypeIdWithoutSignatureStatement{
        "SELECT name, signalDeclarationId FROM signalDeclarations WHERE typeId=? ORDER BY name",
        database};
    mutable ReadStatement<3> selectSignalParameterDeclarationsStatement{
        "SELECT json_extract(json_each.value, '$.n'), json_extract(json_each.value, '$.tn'), "
        "json_extract(json_each.value, '$.tr') FROM signalDeclarations, "
        "json_each(signalDeclarations.signature) WHERE signalDeclarationId=?",
        database};
    WriteStatement insertSignalDeclarationStatement{
        "INSERT INTO signalDeclarations(typeId, name, signature) VALUES(?1, ?2, ?3)", database};
    WriteStatement updateSignalDeclarationStatement{
        "UPDATE signalDeclarations SET  signature=?2 WHERE signalDeclarationId=?1", database};
    WriteStatement deleteSignalDeclarationStatement{
        "DELETE FROM signalDeclarations WHERE signalDeclarationId=?", database};
    mutable ReadStatement<3> selectEnumerationDeclarationsForTypeIdStatement{
        "SELECT name, enumeratorDeclarations, enumerationDeclarationId FROM "
        "enumerationDeclarations WHERE typeId=? ORDER BY name",
        database};
    mutable ReadStatement<2> selectEnumerationDeclarationsForTypeIdWithoutEnumeratorDeclarationsStatement{
        "SELECT name, enumerationDeclarationId FROM enumerationDeclarations WHERE typeId=? ORDER "
        "BY name",
        database};
    mutable ReadStatement<3> selectEnumeratorDeclarationStatement{
        "SELECT json_each.key, json_each.value, json_each.type!='null' FROM "
        "enumerationDeclarations, json_each(enumerationDeclarations.enumeratorDeclarations) WHERE "
        "enumerationDeclarationId=?",
        database};
    WriteStatement insertEnumerationDeclarationStatement{
        "INSERT INTO enumerationDeclarations(typeId, name, enumeratorDeclarations) VALUES(?1, ?2, "
        "?3)",
        database};
    WriteStatement updateEnumerationDeclarationStatement{
        "UPDATE enumerationDeclarations SET  enumeratorDeclarations=?2 WHERE "
        "enumerationDeclarationId=?1",
        database};
    WriteStatement deleteEnumerationDeclarationStatement{
        "DELETE FROM enumerationDeclarations WHERE enumerationDeclarationId=?", database};
    mutable ReadWriteStatement<1> insertImportStatement{
        "INSERT INTO imports(name, version, sourceId) VALUES(?1, ?2, ?3) RETURNING importId",
        database};
    WriteStatement updateImportStatement{"UPDATE imports SET sourceId=?2 WHERE importId=?1", database};
    WriteStatement deleteImportStatement{"DELETE FROM imports WHERE importId=?", database};
    mutable ReadStatement<1> selectImportIdByNameStatement{
        "SELECT importId FROM imports WHERE name=? ORDER BY version DESC LIMIT 1", database};
    mutable ReadStatement<1> selectImportIdByNameAndVersionStatement{
        "SELECT importId FROM imports WHERE name=? AND version=?", database};
    mutable ReadStatement<4> selectAllImportsStatement{
        "SELECT name, version, sourceId, importId FROM imports ORDER BY name, version", database};
    WriteStatement insertImportDependencyStatement{
        "INSERT INTO importDependencies(importId, parentImportId) VALUES(?1, ?2)", database};
    WriteStatement deleteImportDependencyStatement{
        "DELETE FROM importDependencies WHERE importId=?1 AND parentImportId=?2", database};
    mutable ReadStatement<2> selectAllImportDependenciesStatement{
        "SELECT importId, parentImportId FROM importDependencies ORDER BY importId, parentImportId",
        database};
    mutable ReadStatement<2> selectImportsForThatDependentOnThisImportIdStatement{
        "SELECT name, version FROM importDependencies JOIN imports ON "
        "importDependencies.parentImportId = imports.importId WHERE importDependencies.importId=?",
        database};
    mutable ReadStatement<1> selectTypeIdsForImportIdStatement{
        "SELECT typeId FROM types WHERE importId=?", database};
    mutable ReadStatement<1> selectTypeIdByImportIdAndNameStatement{
        "SELECT typeId FROM types WHERE importId=?1 and name=?2", database};
    mutable ReadStatement<1> selectTypeIdByImportIdsAndExportedNameStatement{
        "SELECT typeId FROM exportedTypes WHERE importId IN carray(?1, ?2, 'int64') AND name=?3",
        database};
    mutable ReadStatement<1> selectTypeIdByImportIdAndExportedNameStatement{
        "SELECT typeId FROM exportedTypes WHERE importId=?1 AND name=?2", database};
    mutable ReadStatement<1> fetchImportDependencyIdsStatement{
        "WITH RECURSIVE "
        "  importIds(importId) AS ("
        "      SELECT value FROM carray(?1, ?2, 'int64') "
        "    UNION "
        "      SELECT parentImportId FROM importDependencies JOIN importIds USING(importId)) "
        "SELECT importId FROM importIds",
        database};
};

} // namespace QmlDesigner
