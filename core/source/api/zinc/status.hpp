/**
 * FILE : status.hpp
 *
 * C++ function status/error codes.
 *
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is OpenCMZN-Zinc Library.
 *
 * The Initial Developer of the Original Code is
 * Auckland Uniservices Ltd, Auckland, New Zealand.
 * Portions created by the Initial Developer are Copyright (C) 2013
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef CMZN_STATUS_HPP
#define CMZN_STATUS_HPP

#include "zinc/status.h"

namespace OpenCMISS
{
namespace Zinc
{

enum Status
{
	ERROR_MEMORY = CMZN_ERROR_MEMORY,
		/*!< failed to allocate memory. */
	ERROR_ARGUMENT = CMZN_ERROR_ARGUMENT,
		/*!< invalid argument(s) passed to API function. Only reported for new APIs. */
	ERROR_GENERAL = CMZN_ERROR_GENERAL,
		/*!< unspecified error occurred. Can include invalid argument(s) for old APIs. */
	OK = CMZN_OK
		/*!< value to be returned on success */
};

} // namespace Zinc
}

#endif
