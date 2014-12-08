/**
 * FILE : write_fieldml.cpp
 * 
 * FieldML 0.5 model writer implementation.
 */
/* OpenCMISS-Zinc Library
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstring>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include "zinc/core.h"
#include "zinc/element.h"
#include "zinc/field.h"
#include "zinc/fieldcache.h"
#include "zinc/fieldfiniteelement.h"
#include "zinc/fieldmodule.h"
#include "zinc/node.h"
#include "zinc/region.h"
#include "zinc/status.h"
#include "computed_field/computed_field.h"
#include "computed_field/computed_field_finite_element.h"
#include "datastore/labels.hpp"
#include "datastore/map.hpp"
#include "datastore/mapindexing.hpp"
#include "field_io/fieldml_common.hpp"
#include "field_io/write_fieldml.hpp"
#include "finite_element/finite_element.h"
#include "finite_element/finite_element_basis.h"
#include "finite_element/finite_element_region.h"
#include "general/debug.h"
#include "general/message.h"
#include "general/mystring.h"
#include "general/refcounted.hpp"
#include "general/refhandle.hpp"
#include "region/cmiss_region.h"
#include "fieldml_api.h"
#include "FieldmlIoApi.h"

namespace {

const FmlObjectHandle FML_INVALID_OBJECT_HANDLE = (const FmlObjectHandle)FML_INVALID_HANDLE;

struct OutputBasisData
{
	FmlObjectHandle fmlBasisEvaluator;
	FmlObjectHandle fmlBasisParametersType;
	FmlObjectHandle fmlBasisLocalNodeType;
	HDsLabels localNodeLabels;

	OutputBasisData()	:
		fmlBasisEvaluator(FML_INVALID_OBJECT_HANDLE),
		fmlBasisParametersType(FML_INVALID_OBJECT_HANDLE),
		fmlBasisLocalNodeType(FML_INVALID_OBJECT_HANDLE)
	{
	}

	OutputBasisData(FmlSessionHandle fmlSession, FmlObjectHandle fmlBasisEvaluatorIn,
			FmlObjectHandle fmlBasisParametersTypeIn, FmlObjectHandle fmlBasisLocalNodeTypeIn)	:
		fmlBasisEvaluator(fmlBasisEvaluatorIn),
		fmlBasisParametersType(fmlBasisParametersTypeIn),
		fmlBasisLocalNodeType(fmlBasisLocalNodeTypeIn),
		localNodeLabels(new DsLabels())
	{
		// assume parameters/local node type has simple contiguous range
		FmlEnsembleValue min = Fieldml_GetEnsembleMembersMin(fmlSession, this->fmlBasisLocalNodeType);
		FmlEnsembleValue max = Fieldml_GetEnsembleMembersMax(fmlSession, this->fmlBasisLocalNodeType);
		this->localNodeLabels->addLabelsRange(min, max);
		char *localNodesName = Fieldml_GetObjectName(fmlSession, this->fmlBasisLocalNodeType);
		if (!localNodesName)
			localNodesName = Fieldml_GetObjectDeclaredName(fmlSession, this->fmlBasisLocalNodeType);
		this->localNodeLabels->setName(localNodesName);
		Fieldml_FreeString(localNodesName);
	}
};

struct ElementFieldComponentTemplate
{
	OutputBasisData *basisData;
	HDsLabels elementLabels;
	HDsMapInt localToGlobalNode;
	HDsMapIndexing localToGlobalNodeIndexing;
	std::vector<int> feLocalNodeIndexes;
	std::vector<int> feScaleFactorIndexes;
	std::string name;
	FmlObjectHandle fmlElementTemplateEvaluator; // set once added as FieldML object

	ElementFieldComponentTemplate() :
		basisData(0),
		fmlElementTemplateEvaluator(FML_INVALID_OBJECT_HANDLE)
	{
	}

	ElementFieldComponentTemplate(OutputBasisData *basisDataIn, DsLabels& elementLabelsIn) :
		basisData(basisDataIn),
		elementLabels(cmzn::Access(&elementLabelsIn)),
		feLocalNodeIndexes(basisDataIn->localNodeLabels->getSize()),
		feScaleFactorIndexes(basisDataIn->localNodeLabels->getSize()),
		fmlElementTemplateEvaluator(FML_INVALID_OBJECT_HANDLE)
	{
	}

	~ElementFieldComponentTemplate()
	{
	}

	int createLocalToGlobalNode()
	{
		DsLabels *labelsArray[2] = { cmzn::GetImpl(this->elementLabels), cmzn::GetImpl(this->basisData->localNodeLabels) };
		cmzn::SetImpl(this->localToGlobalNode, DsMap<int>::create(2, labelsArray));
		cmzn::SetImpl(this->localToGlobalNodeIndexing, this->localToGlobalNode ? this->localToGlobalNode->createIndexing() : 0);
		return (this->localToGlobalNodeIndexing) ? CMZN_OK : CMZN_ERROR_GENERAL;
	}

};

struct FieldComponentTemplate : public cmzn::RefCounted
{
	std::vector<ElementFieldComponentTemplate*> elementTemplates;
	HDsLabels elementLabels;
	HDsMapInt elementTemplateMap;
	HDsMapIndexing mapIndexing;
	std::string name;
	FmlObjectHandle fmlFieldTemplateEvaluator; // set once added as FieldML object

private:
	FieldComponentTemplate() :
		fmlFieldTemplateEvaluator(FML_INVALID_OBJECT_HANDLE)
	{
	}
	FieldComponentTemplate(const FieldComponentTemplate&); // not implemented
	FieldComponentTemplate& operator=(const FieldComponentTemplate&); // not implemented

public:
	FieldComponentTemplate(DsLabels* elementLabelsIn) :
		RefCounted(),
		elementLabels(cmzn::Access(elementLabelsIn)),
		elementTemplateMap(DsMap<int>::create(1, &elementLabelsIn)),
		mapIndexing(elementTemplateMap->createIndexing()),
		fmlFieldTemplateEvaluator(FML_INVALID_OBJECT_HANDLE)
	{
	}

	~FieldComponentTemplate()
	{
	}

	// makes a deep copy of the template with a clone of the elementTemplateMap
	FieldComponentTemplate *clone()
	{
		FieldComponentTemplate *newTemplate = new FieldComponentTemplate();
		if (newTemplate)
		{
			newTemplate->elementLabels = this->elementLabels;
			newTemplate->elementTemplates = this->elementTemplates;
			cmzn::SetImpl(newTemplate->elementTemplateMap, this->elementTemplateMap->clone());
			if (newTemplate->elementTemplateMap)
				cmzn::SetImpl(newTemplate->mapIndexing, newTemplate->elementTemplateMap->createIndexing());
			else
				cmzn::Deaccess(newTemplate);
		}
		return newTemplate;
	}

	int setElementTemplate(DsLabelIndex elementIndex, ElementFieldComponentTemplate* elementTemplate)
	{
		int i = 0;
		int size = static_cast<int>(this->elementTemplates.size());
		for (; i < size; ++i)
			if (this->elementTemplates[i] == elementTemplate)
				break;
		if (i == size)
			this->elementTemplates.push_back(elementTemplate);
		this->mapIndexing->setEntryIndex(*(this->elementLabels), elementIndex);
		++i;
		if (this->elementTemplateMap->setValues(*(this->mapIndexing), 1, &i))
			return CMZN_OK;
		return CMZN_ERROR_GENERAL;
	}

};

typedef cmzn::RefHandle<FieldComponentTemplate> HFieldComponentTemplate;

struct OutputFieldData
{
	cmzn_field_id field;
	int componentCount;
	std::string name;
	FE_field *feField;
	std::vector<HFieldComponentTemplate> componentTemplates;
	bool isDefined; // flag set in current working element
	std::vector<ElementFieldComponentTemplate*> elementComponentTemplates;

	OutputFieldData() :
		field(0),
		componentCount(0),
		name(0),
		feField(0)
	{
	}

	OutputFieldData(cmzn_field_id fieldIn, FE_field *feFieldIn) :
		field(fieldIn),
		componentCount(cmzn_field_get_number_of_components(fieldIn)),
		name(""),
		feField(feFieldIn),
		componentTemplates(componentCount),
		isDefined(false),
		elementComponentTemplates(componentCount)
	{
		char *tmpName = cmzn_field_get_name(field);
		this->name = tmpName;
		cmzn_deallocate(tmpName);
		for (int c = 0; c < this->componentCount; ++c)
			elementComponentTemplates[c] = 0;
	}

	~OutputFieldData()
	{
	}
};

};

class FieldMLWriter
{
	cmzn_region *region;
	cmzn_fieldmodule_id fieldmodule;
	const char *location;
	const char *filename;
	char *regionName;
	FmlSessionHandle fmlSession;
	bool verbose;
	int libraryImportSourceIndex;
	std::map<cmzn_field_domain_type,HDsLabels> nodesetLabels;
	std::map<cmzn_field_domain_type,FmlObjectHandle> fmlNodesTypes;
	std::map<cmzn_field_domain_type,FmlObjectHandle> fmlNodesParametersArguments;
	std::vector<HDsLabels> meshLabels; // indexed by dimension
	std::vector<FmlObjectHandle> fmlMeshElementsType;
	std::map<FmlObjectHandle,FmlObjectHandle> typeArgument;
	std::map<FE_basis*,OutputBasisData> outputBasisMap;
	// later: multimap
	std::map<FE_element_field_component*,ElementFieldComponentTemplate*> elementTemplates;

public:
	FieldMLWriter(struct cmzn_region *region, const char *locationIn, const char *filenameIn) :
		region(cmzn_region_access(region)),
		fieldmodule(cmzn_region_get_fieldmodule(region)),
		location(locationIn),
		filename(filenameIn),
		fmlSession(Fieldml_Create(location, /*regionName*/"/")),
		verbose(false),
		libraryImportSourceIndex(-1),
		meshLabels(4),
		fmlMeshElementsType(4)
	{
		Fieldml_SetDebug(fmlSession, /*debug*/verbose);
		for (int i = 0; i < 4; ++i)
			fmlMeshElementsType[i] = FML_INVALID_OBJECT_HANDLE;
	}

	~FieldMLWriter()
	{
		clearElementTemplates();
		Fieldml_Destroy(fmlSession);
		cmzn_fieldmodule_destroy(&fieldmodule);
		cmzn_region_destroy(&region);
	}

	int writeNodeset(cmzn_field_domain_type domainType, bool writeIfEmpty);
	int writeNodesets();

	int getHighestMeshDimension() const;

	int writeMesh(int meshDimension, bool writeIfEmpty);
	int writeMeshFields(int meshDimension);

	int writeFile(const char *pathandfilename);

private:
	FmlObjectHandle libraryImport(const char *remoteName);
	FmlObjectHandle getArgumentForType(FmlObjectHandle fmlType);
	OutputBasisData *getOutputBasisData(FE_basis *feBasis);
	int defineEnsembleFromLabels(FmlObjectHandle fmlEnsembleType, DsLabels& labels);
	template <typename VALUETYPE> FmlObjectHandle defineParametersFromMap(
		DsMap<VALUETYPE>& parameterMap, FmlObjectHandle fmlValueType);
	int getElementFieldComponentTemplate(FE_element_field_component *feComponent,
		DsLabels& elementLabels, ElementFieldComponentTemplate*& elementTemplate);
	FmlObjectHandle writeElementFieldComponentTemplate(ElementFieldComponentTemplate& elementTemplate,
		int meshDimension, std::string& meshName, int& nextElementTemplateNumber);
	FmlObjectHandle writeFieldTemplate(FieldComponentTemplate& fieldTemplate,
		int meshDimension, std::string& meshName, int& nextFieldTemplateNumber, int& nextElementTemplateNumber);
	FmlObjectHandle writeMeshField(std::string& meshName, OutputFieldData& outputField);

	void clearElementTemplates()
	{
		for (std::map<FE_element_field_component*,ElementFieldComponentTemplate*>::iterator iter =
				this->elementTemplates.begin(); iter != this->elementTemplates.end(); ++iter)
			delete iter->second;
		this->elementTemplates.clear();
	}

};

FmlObjectHandle FieldMLWriter::libraryImport(const char *remoteName)
{
	FmlObjectHandle fmlImport = Fieldml_GetObjectByName(this->fmlSession, remoteName);
	if (FML_INVALID_OBJECT_HANDLE != fmlImport)
		return fmlImport;
	if (-1 == this->libraryImportSourceIndex)
	{
		this->libraryImportSourceIndex = Fieldml_AddImportSource(this->fmlSession,
			"http://www.fieldml.org/resources/xml/0.5/FieldML_Library_0.5.xml", "library");
	}
	fmlImport = Fieldml_AddImport(this->fmlSession, libraryImportSourceIndex, remoteName, remoteName);
	if (fmlImport == FML_INVALID_OBJECT_HANDLE)
		display_message(ERROR_MESSAGE, "Failed to import %s from library", remoteName);
	return fmlImport;
}

FmlObjectHandle FieldMLWriter::getArgumentForType(FmlObjectHandle fmlType)
{
	FieldmlHandleType objectType = Fieldml_GetObjectType(this->fmlSession, fmlType);
	if ((objectType != FHT_ENSEMBLE_TYPE) &&
		(objectType != FHT_CONTINUOUS_TYPE) &&
		(objectType != FHT_MESH_TYPE))
		return FML_INVALID_OBJECT_HANDLE;
	std::map<FmlObjectHandle,FmlObjectHandle>::iterator iter = this->typeArgument.find(fmlType);
	if (iter != this->typeArgument.end())
		return iter->second;
	char *objectName = Fieldml_GetObjectName(this->fmlSession, fmlType);
	if (!objectName)
		return FML_INVALID_OBJECT_HANDLE;
	std::string argumentName(objectName);
	argumentName += ".argument";
	FmlObjectHandle fmlArgument = Fieldml_CreateArgumentEvaluator(this->fmlSession, argumentName.c_str(), fmlType);
	this->typeArgument[fmlType] = fmlArgument;
	Fieldml_FreeString(objectName);
	return fmlArgument;
}

OutputBasisData *FieldMLWriter::getOutputBasisData(FE_basis *feBasis)
{
	std::map<FE_basis*,OutputBasisData>::iterator iter = this->outputBasisMap.find(feBasis);
	if (iter != this->outputBasisMap.end())
		return &(iter->second);
	const int *basisType = FE_basis_get_basis_type(feBasis);
	if (!basisType)
		return 0;
	int basisDimension = 0;
	FE_basis_get_dimension(feBasis, &basisDimension);
	enum cmzn_elementbasis_function_type functionType[MAXIMUM_ELEMENT_XI_DIMENSIONS];
	for (int i = 0; i < basisDimension; ++i)
		functionType[i] = FE_basis_get_xi_elementbasis_function_type(feBasis, i);
	
	//const char *interpolator_name = interpolatorName.c_str();
	int basisIndex = -1;
	for (int b = 0; b < numLibraryBases; b++)
	{
		if (libraryBases[b].dimension == basisDimension)
		{
			bool match = true;
			for (int i = 0; i < basisDimension; ++i)
				if (libraryBases[b].functionType[i] != functionType[i])
				{
					match = false;
					break;
				}
			if (match)
			{
				basisIndex = b;
				break;
			}
		}
	}
	if (basisIndex < 0)
	{
		char *description = FE_basis_get_description_string(feBasis);
		display_message(ERROR_MESSAGE, "FieldMLWriter: does not support basis %s", description);
		DEALLOCATE(description);
		return 0;
	}
	FmlObjectHandle fmlBasisEvaluator = this->libraryImport(libraryBases[basisIndex].fieldmlBasisEvaluatorName);
	if (FML_INVALID_OBJECT_HANDLE == fmlBasisEvaluator)
		return 0;
	std::string basisEvaluatorName(libraryBases[basisIndex].fieldmlBasisEvaluatorName);
	// assumes starts with "interpolator."
	std::string basisName = basisEvaluatorName.substr(13, std::string::npos);
	if (verbose)
		display_message(INFORMATION_MESSAGE, "Using basis %s\n", basisName.c_str());
	std::string basisParametersTypeName = "parameters." + basisName;
	FmlObjectHandle fmlBasisParametersType = this->libraryImport(basisParametersTypeName.c_str());
	if (FML_INVALID_OBJECT_HANDLE == fmlBasisParametersType)
		return 0;
	std::string basisParametersArgumentName = basisParametersTypeName + ".argument";
	FmlObjectHandle fmlBasisParametersArgument = this->libraryImport(basisParametersArgumentName.c_str());
	if (FML_INVALID_OBJECT_HANDLE == fmlBasisParametersArgument)
		return 0;
	this->typeArgument[fmlBasisParametersType] = fmlBasisParametersArgument;
	std::string basisParametersComponentTypeName = basisParametersTypeName + ".component";
	FmlObjectHandle fmlBasisParametersComponentType = this->libraryImport(basisParametersComponentTypeName.c_str());
	std::string basisParametersComponentArgumentName = basisParametersComponentTypeName + ".argument";
	FmlObjectHandle fmlBasisParametersComponentArgument = this->libraryImport(basisParametersComponentArgumentName.c_str());
	if ((FML_INVALID_OBJECT_HANDLE == fmlBasisParametersComponentType) ||
		(Fieldml_GetValueType(this->fmlSession, fmlBasisParametersComponentArgument) != fmlBasisParametersComponentType))
		return 0;
	this->typeArgument[fmlBasisParametersComponentType] = fmlBasisParametersComponentArgument;
	OutputBasisData newBasisData(this->fmlSession, fmlBasisEvaluator, fmlBasisParametersType, fmlBasisParametersComponentType);
	this->outputBasisMap[feBasis] = newBasisData;
	iter = this->outputBasisMap.find(feBasis);
	if (iter != this->outputBasisMap.end())
		return &(iter->second);
	return 0;
}

int FieldMLWriter::defineEnsembleFromLabels(FmlObjectHandle fmlEnsembleType, DsLabels& labels)
{
	if (fmlEnsembleType == FML_INVALID_OBJECT_HANDLE)
		return CMZN_ERROR_GENERAL;
	int return_code = CMZN_OK;
	FmlErrorNumber fmlError;
	if (labels.isContiguous())
	{
		DsLabelIdentifier firstIdentifier = labels.getLabelIdentifier(0);
		DsLabelIdentifier lastIdentifier = firstIdentifier + labels.getSize() - 1;
		fmlError = Fieldml_SetEnsembleMembersRange(this->fmlSession, fmlEnsembleType,
			firstIdentifier, lastIdentifier, /*stride*/1);
		if (fmlError != FML_OK)
			return_code = CMZN_ERROR_GENERAL;
	}
	else
	{
		// for non-contiguous use inline range data source
		std::string dataResourceName(labels.getName());
		dataResourceName += ".data.resource";
		FmlObjectHandle fmlDataResource = Fieldml_CreateInlineDataResource(this->fmlSession, dataResourceName.c_str());
		std::string dataSourceName(labels.getName());
		dataSourceName += ".data.source";
		FmlObjectHandle fmlDataSource = Fieldml_CreateArrayDataSource(this->fmlSession, dataSourceName.c_str(), fmlDataResource, /*location*/"0", /*rank*/2);
		DsLabelIdentifierRanges ranges;
		labels.getIdentifierRanges(ranges);
		int sizes[2] = { static_cast<int>(ranges.size()), 2 };
		Fieldml_SetArrayDataSourceRawSizes(this->fmlSession, fmlDataSource, sizes);
		Fieldml_SetArrayDataSourceSizes(this->fmlSession, fmlDataSource, sizes);
		FmlWriterHandle fmlArrayWriter = Fieldml_OpenArrayWriter(this->fmlSession,
			/*objectHandle*/fmlDataSource,
			/*typeHandle*/fmlEnsembleType,
			/*append*/false,
			sizes, // GRC OUTVALUE?
			/*rank*/2
			);
		if (fmlArrayWriter == FML_INVALID_OBJECT_HANDLE)
			return_code = CMZN_ERROR_GENERAL;
		if (CMZN_OK == return_code)
		{
			int numberOfRanges = static_cast<int>(ranges.size());
			for (int i = 0; i < numberOfRanges; ++i)
			{
				const int slabOffsets[] = { i, 0 };
				const int slabSizes[] = { 1, 2 };
				const int range[2] = { ranges[i].first, ranges[i].last };
				FmlIoErrorNumber fmlIoError = Fieldml_WriteIntSlab(fmlArrayWriter, slabOffsets, slabSizes, range);
				if (FML_IOERR_NO_ERROR != fmlIoError)
				{
					return_code = CMZN_ERROR_GENERAL;
					break;
				}
			}
		}
		Fieldml_CloseWriter(fmlArrayWriter);
		if (CMZN_OK == return_code)
		{
			fmlError = Fieldml_SetEnsembleMembersDataSource(this->fmlSession, fmlEnsembleType,
				FML_ENSEMBLE_MEMBER_RANGE_DATA, labels.getSize(), fmlDataSource);
			if (fmlError != FML_OK)
				return_code = CMZN_ERROR_GENERAL;
		}
	}
	return return_code;
}

// template and full specialisations to write different types with template
template <typename VALUETYPE> FmlIoErrorNumber FieldML_WriteSlab(
	FmlWriterHandle writerHandle, const int *offsets, const int *sizes, const VALUETYPE *valueBuffer);

template <> inline FmlIoErrorNumber FieldML_WriteSlab(
	FmlWriterHandle writerHandle, const int *offsets, const int *sizes, const double *valueBuffer)
{
	return Fieldml_WriteDoubleSlab(writerHandle, offsets, sizes, valueBuffer);
}

template <> inline FmlIoErrorNumber FieldML_WriteSlab(
	FmlWriterHandle writerHandle, const int *offsets, const int *sizes, const int *valueBuffer)
{
	return Fieldml_WriteIntSlab(writerHandle, offsets, sizes, valueBuffer);
}

template <typename VALUETYPE> FmlObjectHandle FieldMLWriter::defineParametersFromMap(
	DsMap<VALUETYPE>& parameterMap, FmlObjectHandle fmlValueType)
{
	std::string name = parameterMap.getName();
	std::vector<HDsLabels> sparseLabelsArray;
	std::vector<HDsLabels> denseLabelsArray;
	parameterMap.getSparsity(sparseLabelsArray, denseLabelsArray);
	std::string dataResourceName(name + ".data.resource");
	FmlObjectHandle fmlDataResource = Fieldml_CreateInlineDataResource(this->fmlSession, dataResourceName.c_str());
	const int denseLabelsCount = static_cast<int>(denseLabelsArray.size());
	const int sparseLabelsCount = static_cast<int>(sparseLabelsArray.size());
	std::string dataSourceName(name + ".data.source");
	int return_code = CMZN_OK;
	FmlErrorNumber fmlError;
	FmlObjectHandle fmlDataSource = FML_INVALID_OBJECT_HANDLE;
	FmlObjectHandle fmlKeyDataSource = FML_INVALID_OBJECT_HANDLE;
	if (sparseLabelsCount > 0)
	{
		// when writing to a text bulk data format we want the sparse labels to
		// precede the dense data under those labels (so kept together). This can only
		// be done if both are rank 2. Must confirm than the FieldML API can accept a
		// rank 2 data source for sparse data with more than 1 dense indexes.
		// This requires the second size to match product of dense index sizes.
		// Later: With HDF5 we need separate integer key and real data arrays.
		fmlDataSource = Fieldml_CreateArrayDataSource(this->fmlSession, dataSourceName.c_str(),
			fmlDataResource, /*location*/"1", /*rank*/2);
		std::string indexDataSourceName(name + ".key.data.source");
		fmlKeyDataSource = Fieldml_CreateArrayDataSource(this->fmlSession, indexDataSourceName.c_str(),
			fmlDataResource, /*location*/"1", /*rank*/2);
		int denseSize = 1;
		for (int i = 0; i < denseLabelsCount; ++i)
			denseSize *= denseLabelsArray[i]->getSize();
		// start with 1 record and revise once known
		int numberOfRecords = 1;
		int rawSizes[2] = { numberOfRecords, sparseLabelsCount + denseSize };
		int sizes[2] = { numberOfRecords, denseSize };
		int offsets[2] = { 0, sparseLabelsCount };
		int keySizes[2] = { numberOfRecords, sparseLabelsCount };
		int keyOffsets[2]= { 0, 0 };
		for (int i = 0; i < denseLabelsCount; ++i)
		{
			sizes[i] = denseLabelsArray[i]->getSize();
			offsets[i] = 0;
		}
		Fieldml_SetArrayDataSourceRawSizes(this->fmlSession, fmlDataSource, rawSizes);
		Fieldml_SetArrayDataSourceSizes(this->fmlSession, fmlDataSource, sizes);
		Fieldml_SetArrayDataSourceOffsets(this->fmlSession, fmlDataSource, offsets);
		Fieldml_SetArrayDataSourceRawSizes(this->fmlSession, fmlKeyDataSource, rawSizes);
		Fieldml_SetArrayDataSourceSizes(this->fmlSession, fmlKeyDataSource, keySizes);
		Fieldml_SetArrayDataSourceOffsets(this->fmlSession, fmlKeyDataSource, keyOffsets);
		numberOfRecords = 0;
		HDsMapIndexing mapIndexing(parameterMap.createIndexing());
		for (int i = 0; i < sparseLabelsCount; ++i)
			mapIndexing->setEntryIndex(*sparseLabelsArray[i], DS_LABEL_INDEX_INVALID);
		mapIndexing->resetSparseIterators();
		VALUETYPE *denseValues = new VALUETYPE[denseSize];
		if (denseSize && (!denseValues))
			return_code = CMZN_ERROR_MEMORY;
		else
		{
			std::ostringstream stringStream;
			stringStream << "\n";
			while (parameterMap.incrementSparseIterators(*mapIndexing))
			{
				if (parameterMap.getValues(*mapIndexing, denseSize, denseValues))
				{
					++numberOfRecords;
					for (int i = 0; i < sparseLabelsCount; ++i)
					{
						DsLabelIdentifier identifier = mapIndexing->getSparseIdentifier(i);
						if (i > 0)
							stringStream << " ";
						stringStream << identifier;
					}
					// Future: control numerical format for reals
					for (int i = 0; i < denseSize; ++i)
						stringStream << " " << denseValues[i];
					stringStream << "\n";
				}
				else
				{
					return_code = CMZN_ERROR_GENERAL;
					break;
				}
			}
			std::string sstring = stringStream.str();
			int sstringSize = static_cast<int>(sstring.size());
			fmlError = Fieldml_SetInlineData(this->fmlSession, fmlDataResource, sstring.c_str(), sstringSize);
			if (FML_OK != fmlError)
				return_code = CMZN_ERROR_GENERAL;
		}
		delete[] denseValues;
		rawSizes[0] = numberOfRecords;
		sizes[0] = numberOfRecords;
		keySizes[0] = numberOfRecords;
		Fieldml_SetArrayDataSourceRawSizes(this->fmlSession, fmlDataSource, rawSizes);
		Fieldml_SetArrayDataSourceSizes(this->fmlSession, fmlDataSource, sizes);
		Fieldml_SetArrayDataSourceRawSizes(this->fmlSession, fmlKeyDataSource, rawSizes);
		Fieldml_SetArrayDataSourceSizes(this->fmlSession, fmlKeyDataSource, keySizes);
	}
	else
	{
		fmlDataSource = Fieldml_CreateArrayDataSource(this->fmlSession, dataSourceName.c_str(),
			fmlDataResource, /*location*/"0", /*rank*/denseLabelsCount);
		int *sizes = new int[denseLabelsCount];
		int *offsets = new int[denseLabelsCount];
		for (int i = 0; i < denseLabelsCount; ++i)
		{
			sizes[i] = denseLabelsArray[i]->getSize();
			offsets[i] = 0;
		}
		Fieldml_SetArrayDataSourceRawSizes(this->fmlSession, fmlDataSource, sizes);
		Fieldml_SetArrayDataSourceSizes(this->fmlSession, fmlDataSource, sizes);

		FmlWriterHandle fmlArrayWriter = Fieldml_OpenArrayWriter(this->fmlSession,
			fmlDataSource, fmlValueType, /*append*/false,
			sizes, // GRC OUTVALUE?
			/*rank*/denseLabelsCount
			);

		HDsMapIndexing mapIndexing(parameterMap.createIndexing());
		DsMapAddressType denseValuesCount = mapIndexing->getEntryCount();
		VALUETYPE *values = new VALUETYPE[denseValuesCount];
		if (fmlArrayWriter == FML_INVALID_OBJECT_HANDLE)
			return_code = CMZN_ERROR_GENERAL;
		if (CMZN_OK == return_code)
		{
			if (!parameterMap.getValues(*mapIndexing, denseValuesCount, values))
				return_code = CMZN_ERROR_GENERAL;
			else
			{
				FmlIoErrorNumber fmlIoError = FieldML_WriteSlab(fmlArrayWriter, offsets, sizes, values);
				if (FML_IOERR_NO_ERROR != fmlIoError)
					return_code = CMZN_ERROR_GENERAL;
			}
		}
		Fieldml_CloseWriter(fmlArrayWriter);
		delete[] values;
		delete[] offsets;
		delete[] sizes;
	}
	FmlObjectHandle fmlParameters = FML_INVALID_OBJECT_HANDLE;
	if (CMZN_OK == return_code)
	{
		fmlParameters = Fieldml_CreateParameterEvaluator(this->fmlSession, name.c_str(), fmlValueType);
		fmlError = Fieldml_SetParameterDataDescription(this->fmlSession, fmlParameters,
			(0 == sparseLabelsCount) ? FML_DATA_DESCRIPTION_DENSE_ARRAY : FML_DATA_DESCRIPTION_DOK_ARRAY);
		if (FML_OK != fmlError)
			return_code = CMZN_ERROR_GENERAL;
		fmlError = Fieldml_SetDataSource(this->fmlSession, fmlParameters, fmlDataSource);
		if (FML_OK != fmlError)
			return_code = CMZN_ERROR_GENERAL;
		if (0 < sparseLabelsCount)
		{
			fmlError = Fieldml_SetKeyDataSource(this->fmlSession, fmlParameters, fmlKeyDataSource);
			if (FML_OK != fmlError)
				return_code = CMZN_ERROR_GENERAL;
			for (int i = 0; i < sparseLabelsCount; ++i)
			{
				std::string labelsName = sparseLabelsArray[i]->getName();
				FmlObjectHandle fmlLabelsType = Fieldml_GetObjectByName(this->fmlSession, labelsName.c_str());
				FmlObjectHandle fmlIndexArgument = this->getArgumentForType(fmlLabelsType);
				fmlError = Fieldml_AddSparseIndexEvaluator(this->fmlSession, fmlParameters,
					fmlIndexArgument);
				if (FML_OK != fmlError)
				{
					return_code = CMZN_ERROR_GENERAL;
					break;
				}
			}
		}
		for (int i = 0; i < denseLabelsCount; ++i)
		{
			std::string labelsName = denseLabelsArray[i]->getName();
			FmlObjectHandle fmlLabelsType = Fieldml_GetObjectByName(this->fmlSession, labelsName.c_str());
			FmlObjectHandle fmlIndexArgument = this->getArgumentForType(fmlLabelsType);
			fmlError = Fieldml_AddDenseIndexEvaluator(this->fmlSession, fmlParameters,
				fmlIndexArgument, /*orderHandle*/FML_INVALID_OBJECT_HANDLE);
			if (FML_OK != fmlError)
			{
				return_code = CMZN_ERROR_GENERAL;
				break;
			}
		}
	}
	if (CMZN_OK != return_code)
		return FML_INVALID_OBJECT_HANDLE;
	return fmlParameters;
}

int FieldMLWriter::writeMesh(int meshDimension, bool writeIfEmpty)
{
	int return_code = CMZN_OK;
	cmzn_mesh_id mesh = cmzn_fieldmodule_find_mesh_by_dimension(this->fieldmodule, meshDimension);
	char *name = cmzn_mesh_get_name(mesh);
	FmlErrorNumber fmlError;
	int meshSize = cmzn_mesh_get_size(mesh);
	if (writeIfEmpty || (0 < meshSize))
	{
		int *shapeIds = 0;
		FmlObjectHandle fmlMeshType = Fieldml_CreateMeshType(this->fmlSession, name);
		const char *meshChartName = "xi";
		FmlObjectHandle fmlMeshChartType = Fieldml_CreateMeshChartType(this->fmlSession, fmlMeshType, meshChartName);
		FmlObjectHandle fmlMeshChartComponentsType = FML_INVALID_OBJECT_HANDLE;
		if (fmlMeshChartType == FML_INVALID_OBJECT_HANDLE)
			return_code = CMZN_ERROR_GENERAL;
		else
		{
			if (meshDimension > 1)
			{
				const char *chartComponentsName = "mesh3d.xi.components";
				fmlMeshChartComponentsType = Fieldml_CreateContinuousTypeComponents(
					this->fmlSession, fmlMeshChartType, chartComponentsName, meshDimension);
				fmlError = Fieldml_SetEnsembleMembersRange(this->fmlSession, fmlMeshChartComponentsType,
					1, meshDimension, /*stride*/1);
				if (fmlMeshChartComponentsType == FML_INVALID_OBJECT_HANDLE)
					return_code = CMZN_ERROR_GENERAL;
			}
		}
		const char *meshElementsName = "elements";
		FmlObjectHandle fmlMeshElementsType = Fieldml_CreateMeshElementsType(this->fmlSession, fmlMeshType, meshElementsName);

		cmzn_element_shape_type lastShapeType = CMZN_ELEMENT_SHAPE_TYPE_INVALID;
		int lastShapeId = 0;
		std::vector<cmzn_element_shape_type> shapeTypes;
		HDsLabels elementLabels(new DsLabels());
		shapeIds = new int[meshSize];
		elementLabels->setName(std::string(name) + "." + meshElementsName);
		cmzn_element_id element = 0;
		cmzn_elementiterator_id iter = cmzn_mesh_create_elementiterator(mesh);
		int eIndex = 0;
		while (0 != (element = cmzn_elementiterator_next_non_access(iter)))
		{
			if (DS_LABEL_INDEX_INVALID == elementLabels->createLabel(cmzn_element_get_identifier(element)))
			{
				return_code = CMZN_ERROR_MEMORY;
				break;
			}
			cmzn_element_shape_type shapeType = cmzn_element_get_shape_type(element);
			if (shapeType != lastShapeType)
			{
				const int shapeTypesSize = static_cast<int>(shapeTypes.size());
				for (int i = 0; i < shapeTypesSize; ++i)
					if (shapeTypes[i] == shapeType)
					{
						lastShapeType = shapeType;
						lastShapeId = i + 1;
					}
				if (shapeType != lastShapeType)
				{
					shapeTypes.push_back(shapeType);
					lastShapeType = shapeType;
					lastShapeId = static_cast<int>(shapeTypes.size());
				}
			}
			shapeIds[eIndex] = lastShapeId;
			++eIndex;
		}
		cmzn_elementiterator_destroy(&iter);
		this->meshLabels[meshDimension] = elementLabels;
		this->fmlMeshElementsType[meshDimension] = fmlMeshElementsType;
		if (CMZN_OK == return_code)
			return_code = this->defineEnsembleFromLabels(fmlMeshElementsType, *elementLabels);			
		if (CMZN_OK == return_code)
		{
			// ensure we have argument for mesh type and can find argument for elements and xi type
			// since it uses a special naming pattern e.g. mesh3d.argument.elements/xi
			this->getArgumentForType(fmlMeshType);

			std::string meshElementsArgumentName(name);
			meshElementsArgumentName += ".argument.";
			meshElementsArgumentName += meshElementsName;
			FmlObjectHandle fmlMeshElementsArgument = Fieldml_GetObjectByName(this->fmlSession, meshElementsArgumentName.c_str());
			if (fmlMeshElementsArgument == FML_INVALID_OBJECT_HANDLE)
				return_code = CMZN_ERROR_GENERAL;
			else
				this->typeArgument[fmlMeshElementsType] = fmlMeshElementsArgument;
			std::string meshChartArgumentName(name);
			meshChartArgumentName += ".argument.";
			meshChartArgumentName += meshChartName;
			FmlObjectHandle fmlMeshChartArgument = Fieldml_GetObjectByName(this->fmlSession, meshChartArgumentName.c_str());
			if (fmlMeshChartArgument == FML_INVALID_OBJECT_HANDLE)
				return_code = CMZN_ERROR_GENERAL;
			else
				this->typeArgument[fmlMeshChartType] = fmlMeshChartArgument;

			// set up shape evaluator, single fixed or indirectly mapped
			if (1 == shapeTypes.size())
			{
				const char *shapeName = getFieldmlNameFromElementShape(shapeTypes[0]);
				FmlObjectHandle fmlMeshShapeEvaluator = this->libraryImport(shapeName);
				fmlError = Fieldml_SetMeshShapes(this->fmlSession, fmlMeshType, fmlMeshShapeEvaluator);
				if (fmlError != FML_OK)
					return_code = CMZN_ERROR_GENERAL;
			}
			else
			{
				HDsLabels meshShapeLabels(new DsLabels());
				std::string meshShapeIdsName(name);
				meshShapeIdsName += ".shapeids";
				meshShapeLabels->setName(meshShapeIdsName);
				if (CMZN_OK != meshShapeLabels->addLabelsRange(1, static_cast<int>(shapeTypes.size())))
					return_code = CMZN_ERROR_MEMORY;
				FmlObjectHandle fmlMeshShapeIdsType = Fieldml_CreateEnsembleType(this->fmlSession, meshShapeIdsName.c_str());
				if (CMZN_OK == return_code)
					return_code = this->defineEnsembleFromLabels(fmlMeshShapeIdsType, *meshShapeLabels);
				DsLabels *tmpElementLabels = cmzn::GetImpl(elementLabels);
				HDsMapInt meshShapeMap(DsMap<int>::create(1, &tmpElementLabels));
				std::string meshShapeMapName(name);
				meshShapeMapName += ".shapeids.map";
				meshShapeMap->setName(meshShapeMapName);
				HDsMapIndexing meshShapeIndexing(meshShapeMap->createIndexing());
				if (!meshShapeMap->setValues(*meshShapeIndexing, meshSize, shapeIds))
					return_code = CMZN_ERROR_MEMORY;
				FmlObjectHandle fmlMeshShapeIdsParameters = this->defineParametersFromMap<int>(*meshShapeMap, fmlMeshShapeIdsType);
				if (fmlMeshShapeIdsParameters == FML_INVALID_OBJECT_HANDLE)
					return_code = CMZN_ERROR_GENERAL;
				else
				{
					std::string meshShapeEvaluatorName(name);
					meshShapeEvaluatorName += ".shape";
					FmlObjectHandle fmlBooleanType = this->libraryImport("boolean");
					FmlObjectHandle fmlMeshShapeEvaluator = Fieldml_CreatePiecewiseEvaluator(this->fmlSession,
						meshShapeEvaluatorName.c_str(), fmlBooleanType);
					FmlObjectHandle fmlMeshShapeIdsArgument = this->getArgumentForType(fmlMeshShapeIdsType);
					fmlError = Fieldml_SetIndexEvaluator(this->fmlSession, fmlMeshShapeEvaluator, /*index*/1, fmlMeshShapeIdsArgument);
					if (FML_OK != fmlError)
						return_code = CMZN_ERROR_GENERAL;
					const int shapeTypesSize = static_cast<int>(shapeTypes.size());
					for (int i = 0; i < shapeTypesSize; ++i)
					{
						const char *shapeName = getFieldmlNameFromElementShape(shapeTypes[i]);
						FmlObjectHandle fmlShapeEvaluator = this->libraryImport(shapeName);
						fmlError = Fieldml_SetEvaluator(this->fmlSession, fmlMeshShapeEvaluator, i + 1, fmlShapeEvaluator);
						if (FML_OK != fmlError)
							return_code = CMZN_ERROR_GENERAL;
					}
					fmlError = Fieldml_SetBind(this->fmlSession, fmlMeshShapeEvaluator, fmlMeshShapeIdsArgument, fmlMeshShapeIdsParameters);
					if (FML_OK != fmlError)
						return_code = CMZN_ERROR_GENERAL;
					fmlError = Fieldml_SetMeshShapes(this->fmlSession, fmlMeshType, fmlMeshShapeEvaluator);
					if (fmlError != FML_OK)
						return_code = CMZN_ERROR_GENERAL;
				}
			}
		}
		delete[] shapeIds;
	}
	cmzn_deallocate(name);
	cmzn_mesh_destroy(&mesh);
	return return_code;
}

int FieldMLWriter::getHighestMeshDimension() const
{
	FE_region *fe_region = cmzn_region_get_FE_region(this->region);
	return FE_region_get_highest_dimension(fe_region);
}

int FieldMLWriter::writeNodeset(cmzn_field_domain_type domainType, bool writeIfEmpty)
{
	int return_code = CMZN_OK;
	cmzn_nodeset_id nodeset = cmzn_fieldmodule_find_nodeset_by_field_domain_type(
		this->fieldmodule, domainType);
	char *name = cmzn_nodeset_get_name(nodeset);
	if (writeIfEmpty || (0 < cmzn_nodeset_get_size(nodeset)))
	{
		FmlObjectHandle fmlNodesType = Fieldml_CreateEnsembleType(this->fmlSession, name);
		HDsLabels nodeLabels(new DsLabels());
		nodeLabels->setName(name);
		cmzn_node_id node = 0;
		cmzn_nodeiterator_id iter = cmzn_nodeset_create_nodeiterator(nodeset);
		while (0 != (node = cmzn_nodeiterator_next_non_access(iter)))
		{
			if (DS_LABEL_INDEX_INVALID == nodeLabels->createLabel(cmzn_node_get_identifier(node)))
			{
				return_code = CMZN_ERROR_MEMORY;
				break;
			}
		}
		cmzn_nodeiterator_destroy(&iter);
		if (CMZN_OK == return_code)
			return_code = this->defineEnsembleFromLabels(fmlNodesType, *nodeLabels);
		if (CMZN_OK == return_code)
		{
			this->fmlNodesTypes[domainType] = fmlNodesType;
			this->nodesetLabels[domainType] = nodeLabels;
			FmlObjectHandle fmlNodesArgument = this->getArgumentForType(fmlNodesType);
			FmlObjectHandle fmlRealType = this->libraryImport("real.1d");
			std::string nodeParametersArgumentName(name);
			nodeParametersArgumentName += ".parameters.argument";
			FmlObjectHandle fmlNodesParametersArgument = Fieldml_CreateArgumentEvaluator(
				this->fmlSession, nodeParametersArgumentName.c_str(), fmlRealType);
			FmlErrorNumber fmlError = Fieldml_AddArgument(this->fmlSession, fmlNodesParametersArgument, fmlNodesArgument);
			if (FML_OK != fmlError)
				return_code = CMZN_ERROR_GENERAL;
			this->fmlNodesParametersArguments[domainType] = fmlNodesParametersArgument;
		}
	}
	cmzn_deallocate(name);
	cmzn_nodeset_destroy(&nodeset);
	return return_code;
}

int FieldMLWriter::writeNodesets()
{
	int return_code = CMZN_OK;
	if (CMZN_OK == return_code)
		return_code = this->writeNodeset(CMZN_FIELD_DOMAIN_TYPE_NODES, /*writeIfEmpty*/true);
	//if (CMZN_OK == return_code)
	//	return_code = this->writeNodeset(CMZN_FIELD_DOMAIN_TYPE_DATAPOINTS, /*writeIfEmpty*/false);
	return return_code;
}

int FieldMLWriter::getElementFieldComponentTemplate(FE_element_field_component *feComponent,
	DsLabels& elementLabels, ElementFieldComponentTemplate*& elementTemplate)
{
	elementTemplate = 0;
	std::map<FE_element_field_component*,ElementFieldComponentTemplate*>::iterator iter =
		this->elementTemplates.find(feComponent);
	if (iter != this->elementTemplates.end())
	{
		elementTemplate = iter->second;
		return CMZN_OK;
	}
	FE_basis *feBasis;
	if (!FE_element_field_component_get_basis(feComponent, &feBasis))
		return CMZN_ERROR_GENERAL;
	OutputBasisData *basisData = this->getOutputBasisData(feBasis);
	if (!basisData)
		return CMZN_ERROR_NOT_IMPLEMENTED;
	Global_to_element_map_type mapType;
	if (!FE_element_field_component_get_type(feComponent, &mapType))
		return CMZN_ERROR_GENERAL;
	if (mapType != STANDARD_NODE_TO_ELEMENT_MAP)
	{
		display_message(ERROR_MESSAGE, "FieldMLWriter: Unimplemented map type");
		return CMZN_ERROR_NOT_IMPLEMENTED;
	}
	int numberOfNodes = 0;
	if (!FE_element_field_component_get_number_of_nodes(feComponent, &numberOfNodes) ||
		(numberOfNodes != basisData->localNodeLabels->getSize()))
	{
		display_message(ERROR_MESSAGE, "FieldMLWriter: Invalid number of nodes for basis in map");
		return CMZN_ERROR_GENERAL;
	}
	ElementFieldComponentTemplate newElementTemplate(basisData, elementLabels);
	bool hasScaling = false;
	for (int n = 0; n < numberOfNodes; ++n)
	{
		Standard_node_to_element_map *standardNodeMap;
		if (!FE_element_field_component_get_standard_node_map(feComponent, n, &standardNodeMap))
			return CMZN_ERROR_GENERAL;
		int localNodeIndex = -1;
		int numberOfValues;
		int valueIndex;
		int scaleFactorIndex;
		if (!(Standard_node_to_element_map_get_node_index(standardNodeMap, &localNodeIndex) &&
			Standard_node_to_element_map_get_number_of_nodal_values(standardNodeMap, &numberOfValues) &&
			Standard_node_to_element_map_get_nodal_value_index(standardNodeMap, /*valueNumber*/0, &valueIndex) &&
			Standard_node_to_element_map_get_scale_factor_index(standardNodeMap, /*valueNumber*/0, &scaleFactorIndex)))
		{
			return CMZN_ERROR_GENERAL;
		}
		if ((numberOfValues != 1) || (valueIndex != 0))
		{
			display_message(ERROR_MESSAGE, "FieldMLWriter: Writing derivatives and versions is not implemented");
			return CMZN_ERROR_NOT_IMPLEMENTED;
		}
		newElementTemplate.feLocalNodeIndexes[n] = localNodeIndex;
		newElementTemplate.feScaleFactorIndexes[n] = scaleFactorIndex;
		if (scaleFactorIndex >= 0)
			hasScaling = true;
	}
	if (!hasScaling)
		newElementTemplate.feScaleFactorIndexes.clear();

	// Search for matching ElementFieldComponentTemplate (for sharing between components)
	// Currently only supports one template per basis, so different local nodes and scaling fail.
	for (iter = this->elementTemplates.begin(); iter != this->elementTemplates.end(); ++iter)
	{
		if (iter->second->basisData == basisData)
		{
			elementTemplate = iter->second;
			break;
		}
	}
	if (elementTemplate)
	{
		if (newElementTemplate.feLocalNodeIndexes != elementTemplate->feLocalNodeIndexes)
		{
			display_message(ERROR_MESSAGE, "FieldMLWriter: Use of same basis with different local node indexes is not implemented");
			elementTemplate = 0;
			return CMZN_ERROR_NOT_IMPLEMENTED;
		}
		// GRC this is fairly severe as mixes of template without scale factors and with stored scale factors exist
		// Need to fix this soon
		if (newElementTemplate.feScaleFactorIndexes != elementTemplate->feScaleFactorIndexes)
		{
			display_message(ERROR_MESSAGE, "FieldMLWriter: Use of same basis with different scale factor indexes is not implemented");
			elementTemplate = 0;
			return CMZN_ERROR_NOT_IMPLEMENTED;
		}
	}
	else
	{
		elementTemplate = new ElementFieldComponentTemplate(newElementTemplate);
		if (elementTemplate && (CMZN_OK == elementTemplate->createLocalToGlobalNode()))
			this->elementTemplates[feComponent] = elementTemplate;
		else
		{
			delete elementTemplate;
			elementTemplate = 0;
			return CMZN_ERROR_MEMORY;
		}
	}
	return CMZN_OK;
}

FmlObjectHandle FieldMLWriter::writeElementFieldComponentTemplate(ElementFieldComponentTemplate& elementTemplate,
	int meshDimension, std::string& meshName, int& nextElementTemplateNumber)
{
	if (FML_INVALID_OBJECT_HANDLE != elementTemplate.fmlElementTemplateEvaluator)
		return elementTemplate.fmlElementTemplateEvaluator;
	char temp[20];
	sprintf(temp, "%d", nextElementTemplateNumber);
	++nextElementTemplateNumber;
	std::string elementConnectivityName = meshName + ".connectivity" + temp;
	elementTemplate.localToGlobalNode->setName(elementConnectivityName);
	FmlObjectHandle fmlConnectivity = this->defineParametersFromMap(*(elementTemplate.localToGlobalNode),
		this->fmlNodesTypes[CMZN_FIELD_DOMAIN_TYPE_NODES]);
	if (FML_INVALID_OBJECT_HANDLE == fmlConnectivity)
		return FML_INVALID_OBJECT_HANDLE;
	elementTemplate.name = meshName + ".interpolation" + temp;

	std::string elementDofsName = elementTemplate.name + ".dofs";
	FmlObjectHandle fmlElementDofs = Fieldml_CreateAggregateEvaluator(this->fmlSession, elementDofsName.c_str(), 
		elementTemplate.basisData->fmlBasisParametersType);
	if (FML_INVALID_OBJECT_HANDLE == fmlElementDofs)
		return FML_INVALID_OBJECT_HANDLE;
	FmlErrorNumber fmlError;
	fmlError = Fieldml_SetDefaultEvaluator(this->fmlSession, fmlElementDofs,
		this->fmlNodesParametersArguments[CMZN_FIELD_DOMAIN_TYPE_NODES]);
	if (FML_OK != fmlError)
		return FML_INVALID_OBJECT_HANDLE;
	fmlError = Fieldml_SetIndexEvaluator(this->fmlSession, fmlElementDofs, /*index*/1,
		this->getArgumentForType(elementTemplate.basisData->fmlBasisLocalNodeType));
	if (FML_OK != fmlError)
		return FML_INVALID_OBJECT_HANDLE;
	fmlError = Fieldml_SetBind(this->fmlSession, fmlElementDofs,
		this->getArgumentForType(this->fmlNodesTypes[CMZN_FIELD_DOMAIN_TYPE_NODES]), fmlConnectivity);
	if (FML_OK != fmlError)
		return FML_INVALID_OBJECT_HANDLE;

	this->libraryImport("real.1d"); // implicit type of interpolator
	elementTemplate.fmlElementTemplateEvaluator = Fieldml_CreateReferenceEvaluator(this->fmlSession,
		elementTemplate.name.c_str(), elementTemplate.basisData->fmlBasisEvaluator);
	FmlObjectHandle fmlMeshType = Fieldml_GetObjectByName(this->fmlSession, meshName.c_str());
	FmlObjectHandle fmlMeshChartType = Fieldml_GetMeshChartType(this->fmlSession, fmlMeshType);
	FmlObjectHandle fmlMeshChartArgument = getArgumentForType(fmlMeshChartType);

	FmlObjectHandle fmlChartArgument =
		(3 == meshDimension) ? this->libraryImport("chart.3d.argument") :
		(2 == meshDimension) ? this->libraryImport("chart.2d.argument") :
		this->libraryImport("chart.1d.argument");
	fmlError = Fieldml_SetBind(this->fmlSession, elementTemplate.fmlElementTemplateEvaluator,
		fmlChartArgument, fmlMeshChartArgument);
	if (FML_OK != fmlError)
		return FML_INVALID_OBJECT_HANDLE;
	fmlError = Fieldml_SetBind(this->fmlSession, elementTemplate.fmlElementTemplateEvaluator,
		this->getArgumentForType(elementTemplate.basisData->fmlBasisParametersType), fmlElementDofs);
	if (FML_OK != fmlError)
		return FML_INVALID_OBJECT_HANDLE;
	return elementTemplate.fmlElementTemplateEvaluator;
}

FmlObjectHandle FieldMLWriter::writeFieldTemplate(FieldComponentTemplate& fieldTemplate,
	int meshDimension, std::string& meshName, int& nextFieldTemplateNumber, int& nextElementTemplateNumber)
{
	if (FML_INVALID_OBJECT_HANDLE != fieldTemplate.fmlFieldTemplateEvaluator)
		return fieldTemplate.fmlFieldTemplateEvaluator;
	const int elementTemplateCount = static_cast<int>(fieldTemplate.elementTemplates.size());

	char temp[50];
	sprintf(temp, "%d", nextFieldTemplateNumber);
	++nextFieldTemplateNumber;
	fieldTemplate.name = meshName + ".template" + temp;
	FmlObjectHandle fmlRealType = this->libraryImport("real.1d");
	FmlErrorNumber fmlError;
	if ((1 == elementTemplateCount) && fieldTemplate.elementTemplateMap->isDenseAndComplete())
	{
		// simple case for constant element function defined over entire mesh
		fieldTemplate.fmlFieldTemplateEvaluator =
			Fieldml_CreatePiecewiseEvaluator(this->fmlSession, fieldTemplate.name.c_str(), fmlRealType);
		FmlObjectHandle fmlMeshElementsArgument = this->getArgumentForType(this->fmlMeshElementsType[meshDimension]);
		FmlObjectHandle fmlElementEvaluator = this->writeElementFieldComponentTemplate(
			*(fieldTemplate.elementTemplates[0]), meshDimension, meshName, nextElementTemplateNumber);
		fmlError = Fieldml_SetIndexEvaluator(this->fmlSession,
			fieldTemplate.fmlFieldTemplateEvaluator, /*index*/1, fmlMeshElementsArgument);
		if (FML_OK != fmlError)
			return FML_INVALID_OBJECT_HANDLE;
		fmlError = Fieldml_SetDefaultEvaluator(this->fmlSession, fieldTemplate.fmlFieldTemplateEvaluator, fmlElementEvaluator);
		if (FML_OK != fmlError)
			return FML_INVALID_OBJECT_HANDLE;
	}
	else
	{
		HDsLabels elementFunctionIds(new DsLabels());
		if (!elementFunctionIds)
			return FML_INVALID_OBJECT_HANDLE;
		elementFunctionIds->addLabelsRange(1, static_cast<DsLabelIdentifier>(elementTemplateCount));
		std::string elementFunctionIdsName = fieldTemplate.name + ".functionids";
		elementFunctionIds->setName(elementFunctionIdsName);
		FmlObjectHandle fmlElementFunctionIdsType = Fieldml_CreateEnsembleType(this->fmlSession, elementFunctionIdsName.c_str());
		int return_code = this->defineEnsembleFromLabels(fmlElementFunctionIdsType, *elementFunctionIds);
		if (CMZN_OK != return_code)
			return FML_INVALID_OBJECT_HANDLE;
		FmlObjectHandle fmlElementFunctionIdsArgument = this->getArgumentForType(fmlElementFunctionIdsType);
		fieldTemplate.elementTemplateMap->setName(fieldTemplate.name + ".functionmap");
		FmlObjectHandle fmlElementFunctionsIdMap = this->defineParametersFromMap(*fieldTemplate.elementTemplateMap, fmlElementFunctionIdsType);
		fieldTemplate.fmlFieldTemplateEvaluator =
			Fieldml_CreatePiecewiseEvaluator(this->fmlSession, fieldTemplate.name.c_str(), fmlRealType);
		fmlError = Fieldml_SetIndexEvaluator(this->fmlSession, fieldTemplate.fmlFieldTemplateEvaluator,
			/*index*/1, fmlElementFunctionIdsArgument);
		if (FML_OK != fmlError)
			return FML_INVALID_OBJECT_HANDLE;
		fmlError = Fieldml_SetBind(this->fmlSession, fieldTemplate.fmlFieldTemplateEvaluator,
			fmlElementFunctionIdsArgument, fmlElementFunctionsIdMap);
		if (FML_OK != fmlError)
			return FML_INVALID_OBJECT_HANDLE;
		for (int i = 0; i < elementTemplateCount; ++i)
		{
			FmlObjectHandle fmlElementEvaluator = this->writeElementFieldComponentTemplate(
				*(fieldTemplate.elementTemplates[i]), meshDimension, meshName, nextElementTemplateNumber);
			fmlError = Fieldml_SetEvaluator(this->fmlSession, fieldTemplate.fmlFieldTemplateEvaluator,
				static_cast<FmlEnsembleValue>(i + 1), fmlElementEvaluator);
			if (FML_OK != fmlError)
				return FML_INVALID_OBJECT_HANDLE;
		}
	}
	return fieldTemplate.fmlFieldTemplateEvaluator;
}

FmlObjectHandle FieldMLWriter::writeMeshField(std::string&, OutputFieldData& outputField)
{
	// get value type
	FmlObjectHandle fmlValueType = FML_INVALID_OBJECT_HANDLE;
	FmlObjectHandle fmlValueComponentsType = FML_INVALID_OBJECT_HANDLE;
	FmlObjectHandle fmlValueComponentsArgument = FML_INVALID_OBJECT_HANDLE;
	const bool isCoordinate = cmzn_field_is_type_coordinate(outputField.field);
	cmzn_field_coordinate_system_type coordinateSystemType = cmzn_field_get_coordinate_system_type(outputField.field);
	std::string valueComponentsTypeName;
	if (isCoordinate && (outputField.componentCount <= 3) &&
		(CMZN_FIELD_COORDINATE_SYSTEM_TYPE_RECTANGULAR_CARTESIAN == coordinateSystemType))
	{
		if (1 == outputField.componentCount)
			fmlValueType = this->libraryImport("coordinates.rc.1d");
		else
		{
			if (2 == outputField.componentCount)
			{
				fmlValueType = this->libraryImport("coordinates.rc.2d");
				valueComponentsTypeName = "coordinates.rc.2d.component";
				fmlValueComponentsType = this->libraryImport(valueComponentsTypeName.c_str());
				fmlValueComponentsArgument = this->libraryImport("coordinates.rc.2d.component.argument");
			}
			else // 3-D
			{
				fmlValueType = this->libraryImport("coordinates.rc.3d");
				valueComponentsTypeName = "coordinates.rc.3d.component";
				fmlValueComponentsType = this->libraryImport(valueComponentsTypeName.c_str());
				fmlValueComponentsArgument = this->libraryImport("coordinates.rc.3d.component.argument");
			}
			this->typeArgument[fmlValueComponentsType] = fmlValueComponentsArgument;
		}
	}
	else
	{
		if (isCoordinate && (CMZN_FIELD_COORDINATE_SYSTEM_TYPE_RECTANGULAR_CARTESIAN != coordinateSystemType))
		{
			char *coordinateSystemName = cmzn_field_coordinate_system_type_enum_to_string(coordinateSystemType);
			display_message(WARNING_MESSAGE, "FieldMLWriter: Field %s written without %s coordinate system attribute(s)",
				outputField.name.c_str(), coordinateSystemName);
			cmzn_deallocate(coordinateSystemName);
		}
		if (1 == outputField.componentCount)
			fmlValueType = this->libraryImport("real.1d");
		else
		{
			fmlValueType = Fieldml_CreateContinuousType(this->fmlSession, outputField.name.c_str());
			valueComponentsTypeName = outputField.name + ".components";
			fmlValueComponentsType = Fieldml_CreateContinuousTypeComponents(
				this->fmlSession, fmlValueType, valueComponentsTypeName.c_str(), outputField.componentCount);
			fmlValueComponentsArgument = this->getArgumentForType(fmlValueComponentsType);
		}
	}
	if ((FML_INVALID_OBJECT_HANDLE == fmlValueType) ||
		((1 < outputField.componentCount) && (FML_INVALID_OBJECT_HANDLE == fmlValueComponentsArgument)))
		return FML_INVALID_OBJECT_HANDLE;

	// write nodal parameters
	HDsLabels nodesLabels(this->nodesetLabels[CMZN_FIELD_DOMAIN_TYPE_NODES]);
	HDsLabels valueComponentsLabels;
	if (1 < outputField.componentCount)
	{
		cmzn::SetImpl(valueComponentsLabels, new DsLabels());
		valueComponentsLabels->setName(valueComponentsTypeName);
		valueComponentsLabels->addLabelsRange(1, outputField.componentCount);
	}
	DsLabels *labelsArray[2] = { cmzn::GetImpl(nodesLabels), cmzn::GetImpl(valueComponentsLabels) };
	HDsMapDouble nodesFieldParametersMap(DsMap<double>::create((1 < outputField.componentCount) ? 2 : 1, labelsArray));
	std::string nodesFieldParametersMapName("nodes.");
	nodesFieldParametersMapName += outputField.name;
	nodesFieldParametersMap->setName(nodesFieldParametersMapName);
	HDsMapIndexing nodesFieldParametersMapIndexing(nodesFieldParametersMap->createIndexing());
	cmzn_nodeset *nodeset = cmzn_fieldmodule_find_nodeset_by_field_domain_type(this->fieldmodule, CMZN_FIELD_DOMAIN_TYPE_NODES);
	cmzn_fieldcache *fieldcache = cmzn_fieldmodule_create_fieldcache(this->fieldmodule);
	HDsLabelIterator nodesLabelsIterator(nodesLabels->createLabelIterator());
	int return_code = CMZN_OK;
	double *values = new double[outputField.componentCount];
	while (nodesLabelsIterator->increment())
	{
		cmzn_node *node = cmzn_nodeset_find_node_by_identifier(nodeset, nodesLabelsIterator->getIdentifier());
		if (!node)
		{
			return_code = CMZN_ERROR_NOT_FOUND;
			break;
		}
		return_code = cmzn_fieldcache_set_node(fieldcache, node);
		cmzn_node_destroy(&node);
		if (CMZN_OK != return_code)
			break;
		return_code = cmzn_field_evaluate_real(outputField.field, fieldcache, outputField.componentCount, values);
		if (CMZN_OK != return_code)
			break;
		nodesFieldParametersMapIndexing->setEntry(*nodesLabelsIterator);
		if (!nodesFieldParametersMap->setValues(*nodesFieldParametersMapIndexing, outputField.componentCount, values))
		{
			return_code = CMZN_ERROR_GENERAL;
			break;
		}
	}
	delete[] values;
	cmzn_fieldcache_destroy(&fieldcache);
	cmzn_nodeset_destroy(&nodeset);
	if (CMZN_OK != return_code)
	{
		display_message(ERROR_MESSAGE, "FieldMLWriter: Can't get nodal parameters for field %s", outputField.name.c_str());
		return FML_INVALID_OBJECT_HANDLE;
	}
	FmlObjectHandle fmlRealType = this->libraryImport("real.1d");
	FmlObjectHandle fmlNodesFieldParameters = this->defineParametersFromMap(*nodesFieldParametersMap, fmlRealType);
	if (fmlNodesFieldParameters == FML_INVALID_OBJECT_HANDLE)
		return FML_INVALID_OBJECT_HANDLE;

	FmlObjectHandle fmlField = FML_INVALID_OBJECT_HANDLE;
	FmlErrorNumber fmlError;
	if (1 == outputField.componentCount)
	{
		// GRC problem: can't set value type for reference evaluator; should be able to cast
		FieldComponentTemplate& fieldTemplate = *(outputField.componentTemplates[0]);
		fmlField = Fieldml_CreateReferenceEvaluator(this->fmlSession, outputField.name.c_str(), fieldTemplate.fmlFieldTemplateEvaluator);
	}
	else
	{
		fmlField = Fieldml_CreateAggregateEvaluator(this->fmlSession, outputField.name.c_str(), fmlValueType);
		bool defaultEvaluator = true;
		for (int c = 1; c < outputField.componentCount; ++c)
		{
			if (outputField.componentTemplates[c - 1]->fmlFieldTemplateEvaluator !=
				outputField.componentTemplates[c]->fmlFieldTemplateEvaluator)
			{
				defaultEvaluator = false;
				break;
			}
		}
		if (defaultEvaluator)
		{
			fmlError = Fieldml_SetDefaultEvaluator(this->fmlSession, fmlField,
				outputField.componentTemplates[0]->fmlFieldTemplateEvaluator);
			if (FML_OK != fmlError)
				return FML_INVALID_OBJECT_HANDLE;
		}
		else
		{
			for (int c = 0; c < outputField.componentCount; ++c)
			{
				fmlError = Fieldml_SetEvaluator(this->fmlSession, fmlField, static_cast<FmlEnsembleValue>(c + 1),
					outputField.componentTemplates[c]->fmlFieldTemplateEvaluator);
				if (FML_OK != fmlError)
					return FML_INVALID_OBJECT_HANDLE;
			}
		}
	}
	fmlError = Fieldml_SetBind(this->fmlSession, fmlField,
		this->fmlNodesParametersArguments[CMZN_FIELD_DOMAIN_TYPE_NODES], fmlNodesFieldParameters);
	if (FML_OK != fmlError)
		return FML_INVALID_OBJECT_HANDLE;
	return fmlField;
}

int FieldMLWriter::writeMeshFields(int meshDimension)
{
	int return_code = CMZN_OK;
	std::vector<OutputFieldData> outputFields;
	cmzn_fielditerator_id fieldIter = cmzn_fieldmodule_create_fielditerator(this->fieldmodule);
	if (!fieldIter)
		return_code = CMZN_ERROR_MEMORY;
	cmzn_field_id field;
	while (0 != (field = cmzn_fielditerator_next_non_access(fieldIter)))
	{
		FE_field *feField = 0;
		if (Computed_field_get_type_finite_element(field, &feField) && feField)
		{
			cmzn_field_finite_element_id field_finite_element = cmzn_field_cast_finite_element(field);
			if (field_finite_element)
			{
				OutputFieldData thisFieldData(field, feField);
				outputFields.push_back(thisFieldData);
			}
			else
			{
				display_message(WARNING_MESSAGE, "FieldMLWriter: Cannot write finite element field %s"
					" because it is not standard real-valued.", get_FE_field_name(feField));
			}
			cmzn_field_finite_element_destroy(&field_finite_element);
		}
	}
	cmzn_fielditerator_destroy(&fieldIter);
	const int outputFieldsCount = static_cast<int>(outputFields.size());

	cmzn_mesh_id mesh = cmzn_fieldmodule_find_mesh_by_dimension(this->fieldmodule, meshDimension);
	char *tmp = cmzn_mesh_get_name(mesh);
	std::string meshName(tmp);
	cmzn_deallocate(tmp);
	if (!((mesh) && (this->meshLabels[meshDimension])))
		return_code = CMZN_ERROR_ARGUMENT;
	DsLabels& elementLabels = *(this->meshLabels[meshDimension]);
	cmzn_elementiterator_id elemIter = cmzn_mesh_create_elementiterator(mesh);
	if (!elemIter)
		return_code = CMZN_ERROR_MEMORY;

	cmzn_element_id element;
	int elementNodes[64]; // maximum from tricubic Lagrange basis
	HDsLabelIterator elementLabelIterator(elementLabels.createLabelIterator());
	while ((element = cmzn_elementiterator_next_non_access(elemIter)) && (CMZN_OK == return_code))
	{
		int elementNumber = cmzn_element_get_identifier(element);
		elementLabelIterator->setIndex(elementLabels.findLabelByIdentifier(elementNumber));
		for (int f = 0; (f < outputFieldsCount) && (CMZN_OK == return_code); ++f)
		{
			OutputFieldData& outputField = outputFields[f];
			outputFields[f].isDefined = (0 != FE_field_is_defined_in_element_not_inherited(outputFields[f].feField, element));
			if (!outputField.isDefined)
				continue;
			for (int c = 0; c < outputField.componentCount; ++c)
			{
				FE_element_field_component *feComponent;
				if (!get_FE_element_field_component(element, outputField.feField, c, &feComponent))
				{
					return_code = CMZN_ERROR_GENERAL;
					break;
				}
				ElementFieldComponentTemplate* elementTemplate = 0;
				return_code = this->getElementFieldComponentTemplate(feComponent, elementLabels, elementTemplate);
				if (CMZN_OK != return_code)
				{
					display_message(ERROR_MESSAGE, "FieldMLWriter:  Cannot write definition of field %s at element %d",
						outputField.name.c_str(), elementNumber);
					break;
				}
				outputField.elementComponentTemplates[c] = elementTemplate;
				bool firstUseOfElementTemplate = true;
				for (int oc = 0; oc < c; ++oc)
					if (elementTemplate == outputField.elementComponentTemplates[oc])
					{
						firstUseOfElementTemplate = false;
						break;
					}
				if (firstUseOfElementTemplate)
					for (int of = 0; of < f; ++of)
						if (outputFields[of].isDefined)
							for (int oc = outputFields[of].componentCount - 1; 0 <= oc; --oc)
								if (elementTemplate == outputFields[of].elementComponentTemplates[oc])
								{
									firstUseOfElementTemplate = false;
									break;
								}
				if (firstUseOfElementTemplate)
				{
					// fill in local to global map, check unit scale factors
					OutputBasisData& basisData = *(elementTemplate->basisData);
					const int numberOfNodes = basisData.localNodeLabels->getSize();
					for (int n = 0; n < numberOfNodes; ++n)
					{
						cmzn_node *node = 0;
						if (!get_FE_element_node(element, elementTemplate->feLocalNodeIndexes[n], &node) && (node))
						{
							display_message(ERROR_MESSAGE, "FieldMLWriter:  Missing local node %d for field %s component %d at element %d",
								elementTemplate->feLocalNodeIndexes[n] + 1, outputField.name.c_str(), c, elementNumber);
							return_code = CMZN_ERROR_GENERAL;
							break;
						}
						elementNodes[n] = cmzn_node_get_identifier(node);
					}
					const int numberOfScaleFactorIndexes = static_cast<int>(elementTemplate->feScaleFactorIndexes.size());
					for (int s = 0; s < numberOfScaleFactorIndexes; ++s)
					{
						FE_value scaleFactor;
						if (!get_FE_element_scale_factor(element, elementTemplate->feScaleFactorIndexes[s], &scaleFactor))
						{
							return_code = CMZN_ERROR_GENERAL;
							break;
						}
						if ((scaleFactor < 0.999999) || (scaleFactor > 1.000001))
						{
							display_message(ERROR_MESSAGE, "FieldMLWriter: Non-unit scale factors are not implemented (field %s element %d)",
								outputField.name.c_str(), elementNumber);
							return_code = CMZN_ERROR_NOT_IMPLEMENTED;
							break;
						}
					}
					if (CMZN_OK != return_code)
						break;
					// fill in local to global map
					elementTemplate->localToGlobalNodeIndexing->setEntry(*elementLabelIterator);
					if (!elementTemplate->localToGlobalNode->setValues(
						*(elementTemplate->localToGlobalNodeIndexing), numberOfNodes, elementNodes))
					{
						display_message(ERROR_MESSAGE, "FieldMLWriter: Failed to set local to global nodes in element %d", elementNumber);
						return_code = CMZN_ERROR_GENERAL;
						break;
					}
				}
			} // component
		} // field
		for (int f = 0; (f < outputFieldsCount) && (CMZN_OK == return_code); ++f)
		{
			OutputFieldData& outputField = outputFields[f];
			if (!outputField.isDefined)
				continue;
			for (int c = 0; c < outputField.componentCount; ++c)
			{
				ElementFieldComponentTemplate* elementTemplate = outputField.elementComponentTemplates[c];
				if (!elementTemplate)
					continue; // since cleared after field template matched
				FieldComponentTemplate *oldFieldTemplate = cmzn::GetImpl(outputField.componentTemplates[c]);
				FieldComponentTemplate *newFieldTemplate = 0;
				if (oldFieldTemplate)
				{
					// must copy field template if used by a field not defined on this element
					bool copyFieldTemplate = false;
					for (int of = 0; (of < outputFieldsCount) && (!copyFieldTemplate); ++of)
						if (!outputFields[of].isDefined)
							for (int oc = outputFields[of].componentCount - 1; 0 <= oc; --oc)
								if (cmzn::GetImpl(outputFields[of].componentTemplates[oc]) == oldFieldTemplate)
								{
									copyFieldTemplate = true;
									break;
								}
					// must copy field template if used by another field component with different element template
					for (int of = f; (of < outputFieldsCount) && (!copyFieldTemplate); ++of)
						if (outputFields[of].isDefined)
							for (int oc = outputFields[of].componentCount - 1; 0 <= oc; --oc)
								if ((outputFields[of].elementComponentTemplates[oc]) &&
									(cmzn::GetImpl(outputFields[of].componentTemplates[oc]) == oldFieldTemplate) &&
									(elementTemplate != outputFields[of].elementComponentTemplates[oc]))
								{
									copyFieldTemplate = true;
									break;
								}
					if (copyFieldTemplate)
						newFieldTemplate = oldFieldTemplate->clone();
					else
						newFieldTemplate = cmzn::Access(oldFieldTemplate);
				}
				else
					newFieldTemplate = new FieldComponentTemplate(&elementLabels);
				if (!newFieldTemplate)
				{
					display_message(ERROR_MESSAGE, "FieldMLWriter: Failed to create field template");
					return_code = CMZN_ERROR_MEMORY;
					break;
				}
				newFieldTemplate->setElementTemplate(elementLabelIterator->getIndex(), elementTemplate);
				for (int of = f; of < outputFieldsCount; ++of)
					if (outputFields[of].isDefined)
						for (int oc = outputFields[of].componentCount - 1; 0 <= oc; --oc)
							if ((outputFields[of].elementComponentTemplates[oc] == elementTemplate) &&
								(cmzn::GetImpl(outputFields[of].componentTemplates[oc]) == oldFieldTemplate))
							{
								if (newFieldTemplate != oldFieldTemplate)
									cmzn::SetImpl(outputFields[of].componentTemplates[oc], cmzn::Access(newFieldTemplate));
								outputFields[of].elementComponentTemplates[oc] = 0;
							}
				cmzn::Deaccess(newFieldTemplate);
			} // component
		} // field
	} // element
	cmzn_elementiterator_destroy(&elemIter);
	cmzn_mesh_destroy(&mesh);
	if (CMZN_OK == return_code)
	{
		// write element field component templates
		int nextElementTemplateNumber = 1;
		for (int f = 0; (f < outputFieldsCount) && (CMZN_OK == return_code); ++f)
		{
			OutputFieldData& outputField = outputFields[f];
			if (!outputField.componentTemplates[0])
				continue; // not defined on domain
			for (int c = 0; (c < outputField.componentCount) && (CMZN_OK == return_code); ++c)
			{
				FieldComponentTemplate& fieldTemplate = *(outputField.componentTemplates[c]);
				const int elementTemplateCount = static_cast<int>(fieldTemplate.elementTemplates.size());
				for (int i = 0; i < elementTemplateCount; ++i)
				{
					FmlObjectHandle fmlElementEvaluator = this->writeElementFieldComponentTemplate(
						*(fieldTemplate.elementTemplates[i]), meshDimension, meshName, nextElementTemplateNumber);
					if (FML_INVALID_OBJECT_HANDLE == fmlElementEvaluator)
					{
						return_code = CMZN_ERROR_GENERAL;
						break;
					}
				}
			}
		}
		int nextFieldTemplateNumber = 1;
		// write field component templates
		for (int f = 0; (f < outputFieldsCount) && (CMZN_OK == return_code); ++f)
		{
			OutputFieldData& outputField = outputFields[f];
			if (!outputField.componentTemplates[0])
				continue; // not defined on domain
			for (int c = 0; (c < outputField.componentCount) && (CMZN_OK == return_code); ++c)
			{
				FieldComponentTemplate& fieldTemplate = *(outputField.componentTemplates[c]);
				FmlObjectHandle fmlFieldTemplate = this->writeFieldTemplate(fieldTemplate,
					meshDimension, meshName, nextFieldTemplateNumber, nextElementTemplateNumber);
				if (FML_INVALID_OBJECT_HANDLE == fmlFieldTemplate)
				{
					return_code = CMZN_ERROR_GENERAL;
					break;
				}
			}
		}
		// write fields
		for (int f = 0; f < outputFieldsCount; ++f)
		{
			OutputFieldData& outputField = outputFields[f];
			if (!outputField.componentTemplates[0])
				continue; // not defined on domain
			FmlObjectHandle fmlFieldTemplate = this->writeMeshField(meshName, outputField);
			if (FML_INVALID_OBJECT_HANDLE == fmlFieldTemplate)
			{
				return_code = CMZN_ERROR_GENERAL;
				break;
			}
		}
	}
	clearElementTemplates();
	return return_code;
}

int FieldMLWriter::writeFile(const char *pathandfilename)
{
	FmlErrorNumber fmlError = Fieldml_WriteFile(this->fmlSession, pathandfilename);
	if (FML_OK == fmlError)
		return CMZN_OK;
	return CMZN_ERROR_GENERAL;
}

int write_fieldml_file(struct cmzn_region *region, const char *pathandfilename)
{
	int return_code = CMZN_OK;
	if (region && pathandfilename && (*pathandfilename != '\0'))
	{
		char *location = duplicate_string(pathandfilename);
		char *lastDirSep = strrchr(location, '/');
		char *lastDirSepWin = strrchr(location, '\\');
		if (lastDirSepWin > lastDirSep)
			lastDirSep = lastDirSepWin;
		const char *filename;
		if (lastDirSep)
		{
			*lastDirSep = '\0';
			filename = lastDirSep + 1;
		}
		else
		{
			location[0] = '\0';
			filename = pathandfilename;
		}
		FieldMLWriter fmlWriter(region, location, filename);
		if (CMZN_OK == return_code)
			return_code = fmlWriter.writeNodesets();
		// Currently only writes highest dimension mesh
		int highestMeshDimension = fmlWriter.getHighestMeshDimension();
		if (0 < highestMeshDimension)
		{
			if (CMZN_OK == return_code)
				return_code = fmlWriter.writeMesh(highestMeshDimension, /*writeIfEmpty*/false);
			if (CMZN_OK == return_code)
				return_code = fmlWriter.writeMeshFields(highestMeshDimension);
		}
		if (CMZN_OK == return_code)
			return_code = fmlWriter.writeFile(pathandfilename);
	}
	else
		return_code = CMZN_ERROR_ARGUMENT;
	return return_code;
}