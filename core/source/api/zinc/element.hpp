/***************************************************************************//**
 * FILE : element.hpp
 */
/* OpenCMISS-Zinc Library
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CMZN_ELEMENT_HPP__
#define CMZN_ELEMENT_HPP__

#include "zinc/element.h"
#include "zinc/field.hpp"
#include "zinc/differentialoperator.hpp"
#include "zinc/node.hpp"

namespace OpenCMISS
{
namespace Zinc
{

class Elementbasis
{
private:

	cmzn_elementbasis_id id;

public:

	Elementbasis() : id(0)
	{ }

	// takes ownership of C handle, responsibility for destroying it
	explicit Elementbasis(cmzn_elementbasis_id element_basis_id) :
		id(element_basis_id)
	{ }

	Elementbasis(const Elementbasis& elementBasis) :
		id(cmzn_elementbasis_access(elementBasis.id))
	{ }

	Elementbasis& operator=(const Elementbasis& elementBasis)
	{
		cmzn_elementbasis_id temp_id = cmzn_elementbasis_access(elementBasis.id);
		if (0 != id)
		{
			cmzn_elementbasis_destroy(&id);
		}
		id = temp_id;
		return *this;
	}

	~Elementbasis()
	{
		if (0 != id)
		{
			cmzn_elementbasis_destroy(&id);
		}
	}

	bool isValid()
	{
		return (0 != id);
	}

	enum FunctionType
	{
		FUNCTION_TYPE_INVALID = CMZN_ELEMENTBASIS_FUNCTION_TYPE_INVALID,
		FUNCTION_TYPE_CONSTANT = CMZN_ELEMENTBASIS_FUNCTION_TYPE_CONSTANT,
		FUNCTION_TYPE_LINEAR_LAGRANGE = CMZN_ELEMENTBASIS_FUNCTION_TYPE_LINEAR_LAGRANGE,
		FUNCTION_TYPE_QUADRATIC_LAGRANGE = CMZN_ELEMENTBASIS_FUNCTION_TYPE_QUADRATIC_LAGRANGE,
		FUNCTION_TYPE_CUBIC_LAGRANGE = CMZN_ELEMENTBASIS_FUNCTION_TYPE_CUBIC_LAGRANGE,
		FUNCTION_TYPE_LINEAR_SIMPLEX = CMZN_ELEMENTBASIS_FUNCTION_TYPE_LINEAR_SIMPLEX,   /**< linked on 2 or more dimensions */
		FUNCTION_TYPE_QUADRATIC_SIMPLEX = CMZN_ELEMENTBASIS_FUNCTION_TYPE_QUADRATIC_SIMPLEX, /**< linked on 2 or more dimensions */
		FUNCTION_TYPE_CUBIC_HERMITE = CMZN_ELEMENTBASIS_FUNCTION_TYPE_CUBIC_HERMITE
	};

	cmzn_elementbasis_id getId()
	{
		return id;
	}

	int getDimension()
	{
		return cmzn_elementbasis_get_dimension(id);
	}

	enum FunctionType getFunctionType(int chartComponent)
	{
		return static_cast<FunctionType>(cmzn_elementbasis_get_function_type(id, chartComponent));
	}

	int setFunctionType(int chartComponent, FunctionType functionType)
	{
		return cmzn_elementbasis_set_function_type(id, chartComponent,
			static_cast<cmzn_elementbasis_function_type>(functionType));
	}

	int getNumberOfNodes()
	{
		return cmzn_elementbasis_get_number_of_nodes(id);
	}

	int getNumberOfFunctions()
	{
		return cmzn_elementbasis_get_number_of_functions(id);
	}

};

class Elementtemplate;

class Element
{
friend bool operator==(const Element& a, const Element& b);

private:

	cmzn_element_id id;

public:

	Element() : id(0)
	{ }

	// takes ownership of C handle, responsibility for destroying it
	explicit Element(cmzn_element_id element_id) :
		id(element_id)
	{ }

	Element(const Element& element) :
		id(cmzn_element_access(element.id))
	{ }

	~Element()
	{
		if (0 != id)
		{
			cmzn_element_destroy(&id);
		}
	}

	bool isValid()
	{
		return (0 != id);
	}

	enum FaceType
	{
		FACE_TYPE_INVALID = CMZN_ELEMENT_FACE_TYPE_INVALID,
		FACE_TYPE_ALL = CMZN_ELEMENT_FACE_TYPE_ALL,
		FACE_TYPE_XI1_0 = CMZN_ELEMENT_FACE_TYPE_XI1_0,
		FACE_TYPE_XI1_1 = CMZN_ELEMENT_FACE_TYPE_XI1_1,
		FACE_TYPE_XI2_0 = CMZN_ELEMENT_FACE_TYPE_XI2_0,
		FACE_TYPE_XI2_1 = CMZN_ELEMENT_FACE_TYPE_XI2_1,
		FACE_TYPE_XI3_0 = CMZN_ELEMENT_FACE_TYPE_XI3_0,
		FACE_TYPE_XI3_1 = CMZN_ELEMENT_FACE_TYPE_XI3_1
	};

	Element& operator=(const Element& element)
	{
		cmzn_element_id temp_id = cmzn_element_access(element.id);
		if (0 != id)
		{
			cmzn_element_destroy(&id);
		}
		id = temp_id;
		return *this;
	}

	enum ShapeType
	{
		SHAPE_TYPE_INVALID = CMZN_ELEMENT_SHAPE_TYPE_INVALID,        /**< unspecified shape of known dimension */
		SHAPE_TYPE_LINE = CMZN_ELEMENT_SHAPE_TYPE_LINE,              /**< 1-D: 0 <= xi1 <= 1 */
		SHAPE_TYPE_SQUARE = CMZN_ELEMENT_SHAPE_TYPE_SQUARE,          /**< 2-D: 0 <= xi1,xi2 <= 1 */
		SHAPE_TYPE_TRIANGLE = CMZN_ELEMENT_SHAPE_TYPE_TRIANGLE,      /**< 3-D: 0 <= xi1,xi2; xi1+xi2 <= 1 */
		SHAPE_TYPE_CUBE = CMZN_ELEMENT_SHAPE_TYPE_CUBE,              /**< 3-D: 0 <= xi1,xi2,xi3 <= 1 */
		SHAPE_TYPE_TETRAHEDRON = CMZN_ELEMENT_SHAPE_TYPE_TETRAHEDRON,/**< 3-D: 0 <= xi1,xi2,xi3; xi1+xi2+xi3 <= 1 */
		SHAPE_TYPE_WEDGE12 = CMZN_ELEMENT_SHAPE_TYPE_WEDGE12,        /**< 3-D: 0 <= xi1,xi2; xi1+xi2 <= 1; 0 <= xi3 <= 1 */
		SHAPE_TYPE_WEDGE13 = CMZN_ELEMENT_SHAPE_TYPE_WEDGE13,        /**< 3-D: 0 <= xi1,xi3; xi1+xi3 <= 1; 0 <= xi2 <= 1 */
		SHAPE_TYPE_WEDGE23 = CMZN_ELEMENT_SHAPE_TYPE_WEDGE23         /**< 3-D: 0 <= xi2,xi3; xi2+xi3 <= 1; 0 <= xi1 <= 1 */
	};

	enum PointSamplingMode
	{
		POINT_SAMPLING_MODE_INVALID = CMZN_ELEMENT_POINT_SAMPLING_MODE_INVALID,
		POINT_SAMPLING_MODE_CELL_CENTRES = CMZN_ELEMENT_POINT_SAMPLING_MODE_CELL_CENTRES,
		POINT_SAMPLING_MODE_CELL_CORNERS = CMZN_ELEMENT_POINT_SAMPLING_MODE_CELL_CORNERS,
		POINT_SAMPLING_MODE_CELL_POISSON = CMZN_ELEMENT_POINT_SAMPLING_MODE_CELL_POISSON,
		POINT_SAMPLING_MODE_SET_LOCATION = CMZN_ELEMENT_POINT_SAMPLING_MODE_SET_LOCATION
	};

	cmzn_element_id getId()
	{
		return id;
	}

	int getDimension()
	{
		return cmzn_element_get_dimension(id);
	}

	int getIdentifier()
	{
		return cmzn_element_get_identifier(id);
	}

	int setIdentifier(int identifier)
	{
		return cmzn_element_set_identifier(id, identifier);
	}

	enum ShapeType getShapeType()
	{
		return static_cast<ShapeType>(cmzn_element_get_shape_type(id));
	}

	int merge(Elementtemplate& elementTemplate);

};

inline bool operator==(const Element& a, const Element& b)
{
	return a.id == b.id;
}

class Elementtemplate
{
private:

	cmzn_elementtemplate_id id;

public:

	Elementtemplate() : id(0)
	{ }

	// takes ownership of C handle, responsibility for destroying it
	explicit Elementtemplate(cmzn_elementtemplate_id element_template_id) :
		id(element_template_id)
	{ }

	Elementtemplate(const Elementtemplate& elementTemplate) :
		id(cmzn_elementtemplate_access(elementTemplate.id))
	{ }

	Elementtemplate& operator=(const Elementtemplate& elementTemplate)
	{
		cmzn_elementtemplate_id temp_id = cmzn_elementtemplate_access(elementTemplate.id);
		if (0 != id)
		{
			cmzn_elementtemplate_destroy(&id);
		}
		id = temp_id;
		return *this;
	}

	~Elementtemplate()
	{
		if (0 != id)
		{
			cmzn_elementtemplate_destroy(&id);
		}
	}

	bool isValid()
	{
		return (0 != id);
	}

	cmzn_elementtemplate_id getId()
	{
		return id;
	}

	enum Element::ShapeType getElementShapeType()
	{
		return static_cast<Element::ShapeType>(cmzn_elementtemplate_get_element_shape_type(id));
	}

	int setElementShapeType(enum Element::ShapeType shapeType)
	{
		return cmzn_elementtemplate_set_element_shape_type(id,
			static_cast<cmzn_element_shape_type>(shapeType));
	}

	int getNumberOfNodes()
	{
		return cmzn_elementtemplate_get_number_of_nodes(id);
	}

	int setNumberOfNodes(int numberOfNodes)
	{
		return cmzn_elementtemplate_set_number_of_nodes(id, numberOfNodes);
	}

	int defineFieldSimpleNodal(Field& field, int componentNumber,
		Elementbasis& basis, int nodeIndexesCount, const int *nodeIndexesIn)
	{
		return cmzn_elementtemplate_define_field_simple_nodal(
			id, field.getId(),  componentNumber, basis.getId(),
			nodeIndexesCount, nodeIndexesIn);
	}

	Node getNode(int localNodeIndex)
	{
		return Node(cmzn_elementtemplate_get_node(id, localNodeIndex));
	}

	int setNode(int localNodeIndex, Node& node)
	{
		return cmzn_elementtemplate_set_node(id, localNodeIndex, node.getId());
	}
};

class Elementiterator
{
private:

	cmzn_elementiterator_id id;

public:

	Elementiterator() : id(0)
	{ }

	// takes ownership of C handle, responsibility for destroying it
	explicit Elementiterator(cmzn_elementiterator_id element_iterator_id) :
		id(element_iterator_id)
	{ }

	Elementiterator(const Elementiterator& elementIterator) :
		id(cmzn_elementiterator_access(elementIterator.id))
	{ }

	Elementiterator& operator=(const Elementiterator& elementIterator)
	{
		cmzn_elementiterator_id temp_id = cmzn_elementiterator_access(elementIterator.id);
		if (0 != id)
		{
			cmzn_elementiterator_destroy(&id);
		}
		id = temp_id;
		return *this;
	}

	~Elementiterator()
	{
		if (0 != id)
		{
			cmzn_elementiterator_destroy(&id);
		}
	}

	bool isValid()
	{
		return (0 != id);
	}

	Element next()
	{
		return Element(cmzn_elementiterator_next(id));
	}
};

class Mesh
{

protected:
	cmzn_mesh_id id;

public:

	Mesh() : id(0)
	{ }

	// takes ownership of C handle, responsibility for destroying it
	explicit Mesh(cmzn_mesh_id mesh_id) :	id(mesh_id)
	{ }

	Mesh(const Mesh& mesh) :
		id(cmzn_mesh_access(mesh.id))
	{ }

	~Mesh()
	{
		if (0 != id)
		{
			cmzn_mesh_destroy(&id);
		}
	}

	bool isValid()
	{
		return (0 != id);
	}

	Mesh& operator=(const Mesh& mesh)
	{
		cmzn_mesh_id temp_id = cmzn_mesh_access(mesh.id);
		if (0 != id)
		{
			cmzn_mesh_destroy(&id);
		}
		id = temp_id;
		return *this;
	}

	cmzn_mesh_id getId()
	{
		return id;
	}

	bool containsElement(Element& element)
	{
		return cmzn_mesh_contains_element(id, element.getId());
	}

	Elementtemplate createElementtemplate()
	{
		return Elementtemplate(cmzn_mesh_create_elementtemplate(id));
	}

	Element createElement(int identifier, Elementtemplate& elementTemplate)
	{
		return Element(cmzn_mesh_create_element(id, identifier, elementTemplate.getId()));
	}

	Elementiterator createElementiterator()
	{
		return Elementiterator(cmzn_mesh_create_elementiterator(id));
	}

	int defineElement(int identifier, Elementtemplate& elementTemplate)
	{
		return cmzn_mesh_define_element(id, identifier, elementTemplate.getId());
	}

	int destroyAllElements()
	{
		return cmzn_mesh_destroy_all_elements(id);
	}

	int destroyElement(Element& element)
	{
		 return cmzn_mesh_destroy_element(id, element.getId());
	}

	int destroyElementsConditional(Field& conditionalField)
	{
		return cmzn_mesh_destroy_elements_conditional(id,
			conditionalField.getId());
	}

	Element findElementByIdentifier(int identifier)
	{
		return Element(cmzn_mesh_find_element_by_identifier(id, identifier));
	}

	Differentialoperator getChartDifferentialoperator(int order, int term)
	{
		return Differentialoperator(cmzn_mesh_get_chart_differentialoperator(
			id, order, term));
	}

	int getDimension()
	{
		return cmzn_mesh_get_dimension(id);
	}

	Mesh getMaster()
	{
		return Mesh(cmzn_mesh_get_master(id));
	}

	char *getName()
	{
		return cmzn_mesh_get_name(id);
	}

	int getSize()
	{
		return cmzn_mesh_get_size(id);
	}

	bool match(Mesh& mesh)
	{
		return cmzn_mesh_match(id, mesh.id);
	}

};

class MeshGroup  : public Mesh
{

public:

	// takes ownership of C handle, responsibility for destroying it
	explicit MeshGroup(cmzn_mesh_group_id mesh_id) : Mesh(reinterpret_cast<cmzn_mesh_id>(mesh_id))
	{ }

	cmzn_mesh_group_id getId()
	{
		return (cmzn_mesh_group_id)(id);
	}

	int addElement(Element& element)
	{
		return cmzn_mesh_group_add_element(
			reinterpret_cast<cmzn_mesh_group_id>(id), element.getId());
	}

	int removeAllElements()
	{
		return cmzn_mesh_group_remove_all_elements(reinterpret_cast<cmzn_mesh_group_id>(id));
	}

	int removeElement(Element& element)
	{
		return cmzn_mesh_group_remove_element(reinterpret_cast<cmzn_mesh_group_id>(id),
			element.getId());
	}

	int removeElementsConditional(Field& conditionalField)
	{
		return cmzn_mesh_group_remove_elements_conditional(
			reinterpret_cast<cmzn_mesh_group_id>(id), conditionalField.getId());
	}

};

inline int Element::merge(Elementtemplate& elementTemplate)
{
	return cmzn_element_merge(id, elementTemplate.getId());
}

}  // namespace Zinc
}

#endif /* CMZN_ELEMENT_HPP__ */
