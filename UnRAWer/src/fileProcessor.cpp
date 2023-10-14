/*
 * UnRAWer - camera raw batch processor on top of OpenImageIO
 * Copyright (c) 2023 Erium Vladlen.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "fileProcessor.h"

std::string toLower(const std::string& str) {
    std::string strCopy = str;
    std::transform(strCopy.begin(), strCopy.end(), strCopy.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return strCopy;
}

void getWritableExt(QString* ext, Settings* settings) {
    std::unique_ptr<ImageOutput> probe;
    QString fn = "probename" + *ext;
    probe = ImageOutput::create(fn.toStdString());
    if (probe) {
        LOG(info) << ext->toStdString() << " is writable" << std::endl;
    }
    else {
        LOG(info) << ext->toStdString() << " is readonly" << std::endl;
        LOG(info) << "Output format changed to " << settings->out_formats[settings->defFormat] << std::endl;
        *ext = "." + QString::fromStdString(settings->out_formats[settings->defFormat]);
    }
    probe.reset();
}

QString getExtension(QString& extension, Settings* settings) {
    //QFileInfo fileInfo(fileName);
    //QString extension = "." + fileInfo.completeSuffix();
    extension = extension.toLower();
    switch (settings->fileFormat) {
        //-1 - original, 0 - TIFF, 1 - OpenEXR, 2 - PNG, 3 - JPEG, 4 - JPEG-2000, 5 - PPM
    case 0:
        return ".tif";
    case 1:
        return ".exr";
    case 2:
        return ".png";
    case 3:
        return ".jpg";
    case 4:
        return ".jp2";
    case 5:
        return ".ppm";
    }
    //Only RAW fils are supported
    //No need to check if extension is writable
    //getWritableExt(&extension, settings);
    extension = "." + QString::fromStdString(settings->out_formats[settings->defFormat]);
    return extension;
}

std::tuple<QString, QString, QString, QString> splitPath(const QString& fileName) { // returns path, parent folder, base name, extension
    QFileInfo fileInfo(fileName);
    QString path = fileInfo.absolutePath();
    QDir parentFolder = fileInfo.dir();
    QString baseName = fileInfo.baseName();
    QString extension = "." + fileInfo.completeSuffix();
    return { path, parentFolder.dirName(), baseName, extension };
}

std::optional<std::string> getPresetfromName(const QString& fileName, Settings* settings) {
    QFileInfo fileInfo(fileName);
    QString baseName = fileInfo.baseName();
    QString path = fileInfo.absolutePath();
    // find if path or baseName contains any of settings.lut_Preset strings
    for (auto& lut_preset : settings->lut_Preset) {
        QString lut_preset_key = lut_preset.first.c_str();
        if (path.contains(lut_preset_key, Qt::CaseInsensitive) || baseName.contains(lut_preset_key, Qt::CaseInsensitive)) {
            return lut_preset.first;
        }
    }
    return std::nullopt;
}

std::tuple<QString, QString, QString> getOutName(QString& path, QString& baseName, QString& extension, QString& prest_sfx, Settings* settings) {
    //QFileInfo fileInfo(fileName);
    //QString baseName = fileInfo.baseName();
    //QString path = fileInfo.absolutePath();
    QString outPath = path;
    if (settings->pathPrefix != "") {
		outPath += "/" + QString(settings->pathPrefix.c_str());
	}
    QString outName = baseName;
    QString proc_sfx = "_conv";
    QString outExt;
    if (prest_sfx != "") {
        if (prest_sfx.startsWith("_")) {
            prest_sfx = prest_sfx.replace(QRegularExpression("_{2,}"), "_");
			proc_sfx = prest_sfx;
		}
		else {
			proc_sfx = "_" + prest_sfx;
		}
    }

    outExt = getExtension(extension, settings);

    if (settings->useSbFldr) {
        outPath += "/" + proc_sfx;
    }
    else {
        outName += proc_sfx;
    }
    return { outPath , outName, outExt };
}