//******************************************************************************
// FILE : function_matrix_trace_implementation.cpp
//
// LAST MODIFIED : 10 September 2004
//
// DESCRIPTION :
//==============================================================================

#include <sstream>

#include "computed_variable/function_matrix_trace.hpp"
#include "computed_variable/function_variable.hpp"
#include "computed_variable/function_variable_matrix.hpp"
#include "computed_variable/function_variable_value_specific.hpp"


// module classes
// ==============

// class Function_variable_matrix_trace
// ------------------------------------

EXPORT template<typename Value_type>
class Function_variable_matrix_trace :
	public Function_variable_matrix<Value_type>
//******************************************************************************
// LAST MODIFIED : 10 September 2004
//
// DESCRIPTION :
//==============================================================================
{
	friend class Function_matrix<Value_type>;
	public:
		// constructor
		Function_variable_matrix_trace(
			const boost::intrusive_ptr< Function_matrix_trace<Value_type> >
			function_matrix_trace):
			Function_variable_matrix<Value_type>(function_matrix_trace,1,1){};
		~Function_variable_matrix_trace(){}
	public:
		Function_variable_handle clone() const
		{
			return (Function_variable_handle(
				new Function_variable_matrix_trace<Value_type>(*this)));
		};
		//???DB.  Should operator() and get_entry do an evaluate?
		boost::intrusive_ptr< Function_variable_matrix<Value_type> > operator()(
			Function_size_type row=1,Function_size_type column=1) const
		{
			boost::intrusive_ptr< Function_matrix_trace<Value_type> >
				function_matrix_trace;
			boost::intrusive_ptr< Function_variable_matrix<Value_type> > result(0);

			if ((function_matrix_trace=boost::dynamic_pointer_cast<
				Function_matrix_trace<Value_type>,Function>(function_private))&&
				(row<=number_of_rows())&&(column<=number_of_columns()))
			{
				result=boost::intrusive_ptr< Function_variable_matrix<Value_type> >(
					new Function_variable_matrix_trace<Value_type>(
					function_matrix_trace));
			}

			return (result);
		}
	private:
		// copy constructor
		Function_variable_matrix_trace(
			const Function_variable_matrix_trace<Value_type>& variable):
			Function_variable_matrix<Value_type>(variable){};
};


// global classes
// ==============

// class Function_matrix_trace
// ---------------------------

EXPORT template<typename Value_type>
ublas::matrix<Value_type,ublas::column_major> 
	Function_matrix_trace<Value_type>::constructor_values(1,1);

EXPORT template<typename Value_type>
Function_matrix_trace<Value_type>::Function_matrix_trace(
	const Function_variable_handle& matrix):Function_matrix<Value_type>(
	Function_matrix_trace<Value_type>::constructor_values),matrix_private(0)
//******************************************************************************
// LAST MODIFIED : 10 September 2004
//
// DESCRIPTION :
// Constructor.
//==============================================================================
{
	boost::intrusive_ptr< Function_variable_matrix<Value_type> > matrix_local;

	if (matrix_local=boost::dynamic_pointer_cast<
		Function_variable_matrix<Value_type>,Function_variable>(matrix))
	{
		Function_size_type number_of_rows;

		number_of_rows=matrix_local->number_of_rows();
		if ((0<number_of_rows)&&(number_of_rows==matrix_local->number_of_columns()))
		{
			matrix_private=matrix_local;
			values(0,0)=0;
		}
		else
		{
			throw Function_matrix_trace<Value_type>::Invalid_matrix();
		}
	}
	else
	{
		throw Function_matrix_trace<Value_type>::Invalid_matrix();
	}
}

EXPORT template<typename Value_type>
Function_matrix_trace<Value_type>::~Function_matrix_trace()
//******************************************************************************
// LAST MODIFIED : 10 September 2004
//
// DESCRIPTION :
// Destructor.
//==============================================================================
{
	// do nothing
}

EXPORT template<typename Value_type>
string_handle Function_matrix_trace<Value_type>::get_string_representation()
//******************************************************************************
// LAST MODIFIED : 10 September 2004
//
// DESCRIPTION :
//==============================================================================
{
	string_handle return_string;
	std::ostringstream out;

	if (return_string=new std::string)
	{
		if (matrix_private)
		{
			out << "trace(";
			out << *(matrix_private->get_string_representation());
			out << ")";
		}
		else
		{
			out << "Invalid Function_matrix_trace";
		}
		*return_string=out.str();
	}

	return (return_string);
}

EXPORT template<typename Value_type>
Function_variable_handle Function_matrix_trace<Value_type>::input()
//******************************************************************************
// LAST MODIFIED : 10 September 2004
//
// DESCRIPTION :
//==============================================================================
{
	Function_handle function;
	Function_variable_handle result(0);

	if (matrix_private&&(function=matrix_private->function()))
	{
		result=function->input();
	}

	return (result);
}

EXPORT template<typename Value_type>
Function_variable_handle Function_matrix_trace<Value_type>::output()
//******************************************************************************
// LAST MODIFIED : 10 September 2004
//
// DESCRIPTION :
//==============================================================================
{
	return (Function_variable_handle(
		new Function_variable_matrix_trace<Value_type>(
		boost::intrusive_ptr< Function_matrix_trace<Value_type> >(this))));
}

EXPORT template<typename Value_type>
bool Function_matrix_trace<Value_type>::operator==(
	const Function& function) const
//******************************************************************************
// LAST MODIFIED : 10 September 2004
//
// DESCRIPTION :
// Equality operator.
//==============================================================================
{
	bool result;

	result=false;
	if (this)
	{
		try
		{
			const Function_matrix_trace<Value_type>& function_matrix_trace=
				dynamic_cast<const Function_matrix_trace<Value_type>&>(function);

			result=equivalent(matrix_private,function_matrix_trace.matrix_private);
		}
		catch (std::bad_cast)
		{
			// do nothing
		}
	}

	return (result);
}

EXPORT template<typename Value_type>
Function_handle Function_matrix_trace<Value_type>::evaluate(
	Function_variable_handle atomic_variable)
//******************************************************************************
// LAST MODIFIED : 10 September 2004
//
// DESCRIPTION :
//==============================================================================
{
	boost::intrusive_ptr< Function_variable_matrix<Value_type> >
		atomic_variable_matrix_trace;
	Function_handle result(0);

	if (atomic_variable&&
		equivalent(Function_handle(this),(atomic_variable->function)())&&
		(atomic_variable_matrix_trace=boost::dynamic_pointer_cast<
		Function_variable_matrix_trace<Value_type>,Function_variable>(
		atomic_variable))&&(1==atomic_variable_matrix_trace->row())&&
		(1==atomic_variable_matrix_trace->column()))
	{
		boost::intrusive_ptr< Function_variable_matrix<Value_type> > temp_variable;
		Function_size_type i,number_of_rows;
		ublas::matrix<Value_type,ublas::column_major> result_matrix(1,1);
		Value_type sum,value;

		number_of_rows=matrix_private->number_of_rows();
		i=1;
		sum=0;
		while ((i<=number_of_rows)&&(temp_variable=(*matrix_private)(i,i))&&
			(temp_variable->evaluate())&&(temp_variable->get_entry(value)))
		{
			sum += value;
			i++;
		}
		if (i>number_of_rows);
		{
			result_matrix(0,0)=sum;
			values(0,0)=sum;
			result=Function_handle(new Function_matrix<Value_type>(result_matrix));
		}
	}

	return (result);
}

EXPORT template<typename Value_type>
bool Function_matrix_trace<Value_type>::evaluate_derivative(Scalar&,
	Function_variable_handle,std::list<Function_variable_handle>&)
//******************************************************************************
// LAST MODIFIED : 10 September 2004
//
// DESCRIPTION :
//==============================================================================
{
	return (false);
}

#if !defined (AIX)
template<>
bool Function_matrix_trace<Scalar>::evaluate_derivative(Scalar& derivative,
	Function_variable_handle atomic_variable,
	std::list<Function_variable_handle>& atomic_independent_variables);
#endif // !defined (AIX)

EXPORT template<typename Value_type>
bool Function_matrix_trace<Value_type>::set_value(
	Function_variable_handle atomic_variable,
	Function_variable_handle atomic_value)
//******************************************************************************
// LAST MODIFIED : 10 September 2004
//
// DESCRIPTION :
//==============================================================================
{
	bool result;
	boost::intrusive_ptr< Function_variable_matrix_trace<Value_type> >
		atomic_matrix_variable;
	boost::intrusive_ptr< Function_variable_value_specific<Value_type> >
		value_type;
	Function_handle function;

	result=false;
	if ((atomic_matrix_variable=boost::dynamic_pointer_cast<
		Function_variable_matrix_trace<Value_type>,Function_variable>(
		atomic_variable))&&
		equivalent(Function_handle(this),atomic_matrix_variable->function())&&
		atomic_value&&(atomic_value->value())&&(value_type=
		boost::dynamic_pointer_cast<Function_variable_value_specific<Value_type>,
		Function_variable_value>(atomic_value->value())))
	{
		result=value_type->set(values(0,0),atomic_value);
	}
	if (!result)
	{
		if (function=matrix_private->function())
		{
			result=function->set_value(atomic_variable,atomic_value);
		}
	}

	return (result);
}

EXPORT template<typename Value_type>
Function_handle Function_matrix_trace<Value_type>::get_value(
	Function_variable_handle atomic_variable)
//******************************************************************************
// LAST MODIFIED : 10 September 2004
//
// DESCRIPTION :
//==============================================================================
{
	Function_handle function,result(0);
	boost::intrusive_ptr< Function_variable_matrix_trace<Value_type> >
		atomic_variable_matrix_trace;
	ublas::matrix<Value_type,ublas::column_major> result_matrix(1,1);

	if (atomic_variable&&
		equivalent(Function_handle(this),(atomic_variable->function)())&&
		(atomic_variable_matrix_trace=boost::dynamic_pointer_cast<
		Function_variable_matrix_trace<Value_type>,Function_variable>(
		atomic_variable))&&
		(atomic_variable_matrix_trace->get_entry)(result_matrix(0,0)))
	{
		result=Function_handle(new Function_matrix<Value_type>(result_matrix));
	}
	if (!result)
	{
		if (function=matrix_private->function())
		{
			result=function->get_value(atomic_variable);
		}
	}

	return (result);
}

EXPORT template<typename Value_type>
Function_matrix_trace<Value_type>::Function_matrix_trace(
	const Function_matrix_trace<Value_type>& function_matrix_trace):
	Function_matrix<Value_type>(function_matrix_trace),
	matrix_private(function_matrix_trace.matrix_private){}
//******************************************************************************
// LAST MODIFIED : 10 September 2004
//
// DESCRIPTION :
// Copy constructor.
//==============================================================================

EXPORT template<typename Value_type>
Function_matrix_trace<Value_type>& Function_matrix_trace<Value_type>::operator=(
	const Function_matrix_trace<Value_type>& function_matrix_trace)
//******************************************************************************
// LAST MODIFIED : 10 September 2004
//
// DESCRIPTION :
// Assignment operator.
//==============================================================================
{
	this->matrix_private=function_matrix_trace.matrix_private;
	this->values=function_matrix_trace.values;

	return (*this);
}
