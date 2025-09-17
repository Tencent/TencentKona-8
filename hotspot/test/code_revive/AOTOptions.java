/*
 * Copyright (C) 2022, 2023, Tencent. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

import java.util.Arrays;
import java.util.ArrayList;

public class AOTOptions {
    public String appJar;
    public ArrayList<String> suffix = new ArrayList<String>();
    // Application classes to be archived
    public String[] appClasses;
    public String codeReviveOption;
    public String archiveName;

    public AOTOptions setAppJar(String appJar) {
        this.appJar = appJar;
        return this;
    }

    public AOTOptions setAppClasses(String[] appClasses) {
        this.appClasses = appClasses;
        return this;
    }

    public AOTOptions addSuffix(String... suffix) {
        for (String s : suffix) this.suffix.add(s);
        return this;
    }

    public void setCodeReviveOptions(String option) {
        this.codeReviveOption = option;
    }

    public AOTOptions() {
    }
}
