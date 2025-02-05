/* AbiSource
 * 
 * Copyright (C) 2002 Dom Lachowicz <cinamod@hotmail.com>
 * Copyright (C) 2004 Robert Staudinger <robsta@stereolyzer.net>
 * Copyright (C) 2009 Hubert Figuiere
 * 
 * Copyright (C) 2005 INdT
 * Author: Daniel d'Andrada T. de Carvalho <daniel.carvalho@indt.org.br>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  
 * 02110-1301 USA.
 */

#include "ut_std_string.h"
#include "pd_Document.h"

#include <set>
#include <string>
#include <vector>

// Class definition include
#include "ODe_ManifestWriter.h"
 
// Internal includes
#include "ODe_Common.h"
#include <boost/algorithm/string.hpp>

#include <glib-object.h>
#include <glib.h>
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <gsf/gsf-output.h>
#include <gsf/gsf-outfile.h>
#include <gsf/gsf-outfile-stdio.h>
#include <gsf/gsf-output-memory.h>
#include <gsf/gsf-output-stdio.h>

/**
 * Ensure that the manifest:file-entry XML elements exist for all the parent
 * directories for path. If such an element already has been written (according to
 * pathsAlreadyWritten) then it will not be done again.
 */
void ODe_ManifestWriter::ensureDirectoryManifest( PD_Document* /*pDoc*/,
                                                  GsfOutput* manifest,
                                                  const std::string& path,
                                                  std::set< std::string >& pathsAlreadyWritten )
{
    std::vector<std::string> directories;
    boost::split(directories, path, boost::is_any_of("/"));
    if( !directories.empty() )
    {
        directories.pop_back();
    }

    std::string runningPath;
    for( std::vector<std::string>::iterator iter = directories.begin();
         iter != directories.end(); ++iter )
    {
        runningPath = runningPath + *iter + "/";

        if( !pathsAlreadyWritten.count( runningPath ) )
        { 
            pathsAlreadyWritten.insert( runningPath );

            std::string name = UT_std_string_sprintf(
                " <manifest:file-entry manifest:media-type=\"\" manifest:full-path=\"%s\"/>\n",
                runningPath.c_str() );
            ODe_gsf_output_write(manifest, name.size(),
                                 reinterpret_cast<const guint8 *>(name.c_str()));
            
        }
    }
}


 
/**
 * 
 */
bool ODe_ManifestWriter::writeManifest(PD_Document* pDoc, GsfOutfile* pODT)
{
    // Create META-INF directory
    GsfOutput* meta_inf = gsf_outfile_new_child(pODT, "META-INF", TRUE);
    
    GsfOutput* manifest = gsf_outfile_new_child(
                            GSF_OUTFILE(meta_inf), "manifest.xml", FALSE);

    std::string name;

    static const char * const preamble [] = {
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n",
        "<!DOCTYPE manifest:manifest PUBLIC \"-//OpenOffice.org//DTD Manifest 1.0//EN\" \"Manifest.dtd\">\n",
        "<manifest:manifest xmlns:manifest=\"urn:oasis:names:tc:opendocument:xmlns:manifest:1.0\">\n",
        " <manifest:file-entry manifest:media-type=\"application/vnd.oasis.opendocument.text\" manifest:full-path=\"/\"/>\n",
        " <manifest:file-entry manifest:media-type=\"text/xml\" manifest:full-path=\"content.xml\"/>\n",
        " <manifest:file-entry manifest:media-type=\"text/xml\" manifest:full-path=\"styles.xml\"/>\n",
        " <manifest:file-entry manifest:media-type=\"text/xml\" manifest:full-path=\"meta.xml\"/>\n",
        " <manifest:file-entry manifest:media-type=\"text/xml\" manifest:full-path=\"settings.xml\"/>\n"
    };

    static const char * const postamble [] = {
        "</manifest:manifest>\n"
    };

    typedef std::set< std::string > absolutePathMimeTypes_t;
    static absolutePathMimeTypes_t absolutePathMimeTypes;
    if( absolutePathMimeTypes.empty() )
    {
        absolutePathMimeTypes.insert("application/rdf+xml");
    }
    
    ODe_writeToStream (manifest, preamble, G_N_ELEMENTS(preamble));

    const char* szName;
    std::string mimeType;
    UT_ConstByteBufPtr pByteBuf;
    std::set< std::string > pathsAlreadyWritten;
    
    for (UT_uint32 k = 0;
         (pDoc->enumDataItems(k,
                              NULL,
                              &szName,
                              pByteBuf,
                              &mimeType)); k++) {
                                
        if (!mimeType.empty()) {

            ensureDirectoryManifest( pDoc, manifest, szName, pathsAlreadyWritten );

            std::string automaticPathPrefix = "Pictures/";
            if( absolutePathMimeTypes.count(mimeType) )
                automaticPathPrefix = "";
            std::string extension;
			
            pDoc->getDataItemFileExtension(szName, extension, true);
            name = UT_std_string_sprintf(
                " <manifest:file-entry manifest:media-type=\"%s\" manifest:full-path=\"%s%s%s\"/>\n",
                mimeType.c_str(), automaticPathPrefix.c_str(), szName, extension.c_str());
            
            ODe_gsf_output_write (manifest, name.size(),
                reinterpret_cast<const guint8 *>(name.c_str()));
        }
    }

    ODe_writeToStream (manifest, postamble, G_N_ELEMENTS(postamble));

    ODe_gsf_output_close(manifest);
    ODe_gsf_output_close(meta_inf);

    return true;
}
