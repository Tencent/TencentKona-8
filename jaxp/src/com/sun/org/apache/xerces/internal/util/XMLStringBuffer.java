/*
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 */
/*
 * The Apache Software License, Version 1.1
 *
 *
 * Copyright (c) 1999-2002 The Apache Software Foundation.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The end-user documentation included with the redistribution,
 *    if any, must include the following acknowledgment:
 *       "This product includes software developed by the
 *        Apache Software Foundation (http://www.apache.org/)."
 *    Alternately, this acknowledgment may appear in the software itself,
 *    if and wherever such third-party acknowledgments normally appear.
 *
 * 4. The names "Xerces" and "Apache Software Foundation" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache",
 *    nor may "Apache" appear in their name, without prior written
 *    permission of the Apache Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE APACHE SOFTWARE FOUNDATION OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation and was
 * originally based on software copyright (c) 1999, International
 * Business Machines, Inc., http://www.apache.org.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 */

package com.sun.org.apache.xerces.internal.util;

import com.sun.org.apache.xerces.internal.xni.XMLString;

/**
 * XMLString is a structure used to pass character arrays. However,
 * XMLStringBuffer is a buffer in which characters can be appended
 * and extends XMLString so that it can be passed to methods
 * expecting an XMLString object. This is a safe operation because
 * it is assumed that any callee will <strong>not</strong> modify
 * the contents of the XMLString structure.
 * <p>
 * The contents of the string are managed by the string buffer. As
 * characters are appended, the string buffer will grow as needed.
 * <p>
 * <strong>Note:</strong> Never set the <code>ch</code>,
 * <code>offset</code>, and <code>length</code> fields directly.
 * These fields are managed by the string buffer. In order to reset
 * the buffer, call <code>clear()</code>.
 *
 * @author Andy Clark, IBM
 * @author Eric Ye, IBM
 * @LastModified: Aug 2021
 */
public class XMLStringBuffer
extends XMLString {

    //
    // Data
    //

    //
    // Constructors
    //

    /**
     *
     */
    public XMLStringBuffer() {
        this(DEFAULT_SIZE);
    } // <init>()

    /**
     *
     *
     * @param size
     */
    public XMLStringBuffer(int size) {
        ch = new char[size];
    } // <init>(int)

    /** Constructs a string buffer from a char. */
    public XMLStringBuffer(char c) {
        this(1);
        append(c);
    } // <init>(char)

    /** Constructs a string buffer from a String. */
    public XMLStringBuffer(String s) {
        this(s.length());
        append(s);
    } // <init>(String)

    /** Constructs a string buffer from the specified character array. */
    public XMLStringBuffer(char[] ch, int offset, int length) {
        this(length);
        append(ch, offset, length);
    } // <init>(char[],int,int)

    /** Constructs a string buffer from the specified XMLString. */
    public XMLStringBuffer(XMLString s) {
        this(s.length);
        append(s);
    } // <init>(XMLString)

    //
    // Public methods
    //

    /** Clears the string buffer. */
    public void clear() {
        offset = 0;
        length = 0;
    }

} // class XMLStringBuffer
