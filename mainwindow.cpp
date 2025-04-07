#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QFileInfo>
#include <QProcess>
#include <QDirIterator>
#include <QDesktopServices>
#include <QUrl>
#include <QRegularExpression>
#include <QPushButton>
#include <QHeaderView>
#include <QSet>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QMenu>
#include <QSettings>
#include <QAction>
#include <QTextStream>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->tableWidget->setColumnCount(11);
    ui->tableWidget->setHorizontalHeaderLabels({"Title", "Year", "Decade", "Resolution", "Aspect Ratio", "Quality", "Path", "Size", "Duration", "Language", "Actions"});
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->tableWidget->setSortingEnabled(true);
    ui->tableWidget->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui->tableWidget, &QTableWidget::customContextMenuRequested, this, &MainWindow::showContextMenu);

    addComboBoxItemIfNotExist(ui->comboBoxDecade, "All");
    addComboBoxItemIfNotExist(ui->comboBoxAspectRatio, "All");
    addComboBoxItemIfNotExist(ui->comboBoxQuality, "All");

    QSettings settings("YourCompany", "VideoBrowserApp");
    QString lastFolder = settings.value("lastFolder").toString();
    QString folderPath = QFileDialog::getExistingDirectory(this, "Select Folder", lastFolder);
    if (!folderPath.isEmpty())
    {
        settings.setValue("lastFolder", folderPath);
        processVideos(folderPath);
    }

    connect(ui->comboBoxDecade, &QComboBox::currentTextChanged, this, &MainWindow::filterTable);
    connect(ui->comboBoxAspectRatio, &QComboBox::currentTextChanged, this, &MainWindow::filterTable);
    connect(ui->comboBoxQuality, &QComboBox::currentTextChanged, this, &MainWindow::filterTable);
    connect(ui->exportButton, &QPushButton::clicked, this, &MainWindow::exportToExcel);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::processVideos(const QString &folderPath)
{
    QStringList videoExtensions = {".mp4", ".mkv", ".avi", ".mov", ".flv", ".wmv"};
    int row = 0;

    QSet<QString> decades;
    QSet<QString> aspectRatios;
    QSet<QString> qualities;

    QDirIterator it(folderPath, QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        QString filePath = it.next();
        QFileInfo fileInfo(filePath);
        if (!videoExtensions.contains("." + fileInfo.suffix().toLower()))
            continue;

        QString resolution = getVideoResolution(filePath);
        QString aspectRatio = getAspectRatio(resolution);
        QString folderName = fileInfo.dir().dirName();
        auto [title, year] = parseFolderName(folderName);
        QString decade = getDecade(year);
        QString quality = getVideoQuality(filePath);
        QString duration = getVideoDuration(filePath);
        QString fileSize = getFileSize(filePath);
        QString audioLanguage = getAudioLanguage(filePath);

        decades.insert(decade);
        aspectRatios.insert(aspectRatio);
        qualities.insert(quality);

        ui->tableWidget->insertRow(row);
        ui->tableWidget->setItem(row, 0, new QTableWidgetItem(title));
        ui->tableWidget->setItem(row, 1, new QTableWidgetItem(year));
        ui->tableWidget->setItem(row, 2, new QTableWidgetItem(decade));
        ui->tableWidget->setItem(row, 3, new QTableWidgetItem(resolution));
        ui->tableWidget->setItem(row, 4, new QTableWidgetItem(aspectRatio));
        ui->tableWidget->setItem(row, 5, new QTableWidgetItem(quality));
        ui->tableWidget->setItem(row, 6, new QTableWidgetItem(filePath)); // Store path here
        ui->tableWidget->setItem(row, 7, new QTableWidgetItem(fileSize));
        ui->tableWidget->setItem(row, 8, new QTableWidgetItem(duration));
        ui->tableWidget->setItem(row, 9, new QTableWidgetItem(audioLanguage));

        // Open button
        QPushButton *openButton = new QPushButton("Open");
        connect(openButton, &QPushButton::clicked, this, [filePath]() {
            QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
        });

        // IMDb and Pahe buttons
        QPushButton *imdbButton = new QPushButton("IMDb");
        connect(imdbButton, &QPushButton::clicked, this, [this, title, year]() {
            openImdbPage(title, year);
        });

        QPushButton *paheButton = new QPushButton("Pahe");
        connect(paheButton, &QPushButton::clicked, this, [this, title, year]() {
            openPahePage(title, year);
        });

        QWidget *buttonsWidget = new QWidget();
        QHBoxLayout *layout = new QHBoxLayout(buttonsWidget);
        layout->addWidget(openButton);
        layout->addWidget(imdbButton);
        layout->addWidget(paheButton);
        layout->setContentsMargins(0, 0, 0, 0);
        ui->tableWidget->setCellWidget(row, 10, buttonsWidget);

        row++;
    }

    addComboBoxItemsSorted(ui->comboBoxDecade, decades);
    addComboBoxItemsSorted(ui->comboBoxAspectRatio, aspectRatios, "UltraWide");
    addComboBoxItemsSorted(ui->comboBoxQuality, qualities);

    ui->tableWidget->resizeColumnsToContents();
    ui->tableWidget->resizeRowsToContents();
}

QString MainWindow::getVideoResolution(const QString &filePath)
{
    QProcess ffprobe;
    QStringList args = {"-v", "error", "-select_streams", "v:0", "-show_entries", "stream=width,height", "-of", "csv=p=0", filePath};

    ffprobe.start("ffprobe", args);
    ffprobe.waitForFinished();
    QString output = ffprobe.readAllStandardOutput().trimmed();
    return output.isEmpty() ? "Unknown" : output.replace(",", "x");
}

QString MainWindow::getAspectRatio(const QString &resolution)
{
    if (resolution == "Unknown") return "Unknown";

    QStringList parts = resolution.split("x");
    if (parts.size() != 2) return "Unknown";

    bool ok1, ok2;
    int width = parts[0].toInt(&ok1);
    int height = parts[1].toInt(&ok2);
    if (!ok1 || !ok2 || height == 0) return "Unknown";

    double ratio = static_cast<double>(width) / height;
    return QString::number(ratio, 'f', 2);
}

QString MainWindow::getVideoQuality(const QString &filePath)
{
    QString fileName = QFileInfo(filePath).fileName().toLower();
    if (fileName.contains("2160p") || fileName.contains("4k")) return "4K";
    if (fileName.contains("1080p")) return "1080p";
    if (fileName.contains("720p")) return "720p";
    return "Unknown";
}

QPair<QString, QString> MainWindow::parseFolderName(const QString &folderName)
{
    QRegularExpression re("(.+?) \\((\\d{4})\\)");
    QRegularExpressionMatch match = re.match(folderName);
    if (match.hasMatch()) return {match.captured(1).trimmed(), match.captured(2)};
    return {folderName, "Unknown"};
}

QString MainWindow::getDecade(const QString &year)
{
    bool ok;
    int y = year.toInt(&ok);
    if (ok) return QString::number(y - (y % 10)) + "s";
    return "Unknown";
}

QString MainWindow::getFileSize(const QString &filePath)
{
    QFileInfo info(filePath);
    double sizeInGB = info.size() / (1024.0 * 1024.0 * 1024.0);
    return QString::number(sizeInGB, 'f', 2) + " GB";
}

QString MainWindow::getVideoDuration(const QString &filePath)
{
    QProcess ffprobe;
    QStringList args = {"-v", "error", "-select_streams", "v:0", "-show_entries", "format=duration", "-of", "default=noprint_wrappers=1:nokey=1", filePath};
    ffprobe.start("ffprobe", args);
    ffprobe.waitForFinished();
    bool ok;
    int seconds = ffprobe.readAllStandardOutput().trimmed().toDouble(&ok);
    if (!ok) return "Unknown";
    int h = seconds / 3600;
    int m = (seconds % 3600) / 60;
    int s = seconds % 60;
    return QString("%1:%2:%3").arg(h, 2, 10, QChar('0')).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
}

QString MainWindow::getAudioLanguage(const QString &filePath)
{
    QProcess ffprobe;
    QStringList args = {"-v", "error", "-select_streams", "a:0", "-show_entries", "stream_tags=language", "-of", "default=noprint_wrappers=1:nokey=1", filePath};
    ffprobe.start("ffprobe", args);
    ffprobe.waitForFinished();
    QString lang = ffprobe.readAllStandardOutput().trimmed();
    return lang.isEmpty() ? "Unknown" : lang;
}

void MainWindow::showContextMenu(const QPoint &pos)
{
    QModelIndex index = ui->tableWidget->indexAt(pos);
    if (!index.isValid()) return;

    QMenu menu(this);
    QAction *openFolderAction = menu.addAction("Open Containing Folder");

    connect(openFolderAction, &QAction::triggered, this, [this, index]() {
        QTableWidgetItem *pathItem = ui->tableWidget->item(index.row(), 6);
        if (!pathItem) {
            QMessageBox::warning(this, "Error", "No path information available.");
            return;
        }

        QString filePath = pathItem->text();
        QFileInfo fileInfo(filePath);
        if (!fileInfo.exists()) {
            QMessageBox::warning(this, "Error", "The file does not exist.");
            return;
        }

        QDesktopServices::openUrl(QUrl::fromLocalFile(fileInfo.absolutePath()));
    });

    menu.exec(ui->tableWidget->viewport()->mapToGlobal(pos));
}

void MainWindow::filterTable()
{
    QString selectedDecade = ui->comboBoxDecade->currentText();
    QString selectedAspectRatio = ui->comboBoxAspectRatio->currentText();
    QString selectedQuality = ui->comboBoxQuality->currentText();

    for (int row = 0; row < ui->tableWidget->rowCount(); ++row)
    {
        bool matchDecade = (selectedDecade == "All" || ui->tableWidget->item(row, 2)->text() == selectedDecade);
        bool matchAspectRatio = (selectedAspectRatio == "All" ||
                                 (selectedAspectRatio == "UltraWide" &&
                                  ui->tableWidget->item(row, 4)->text().toDouble() >= 2.2 &&
                                  ui->tableWidget->item(row, 4)->text().toDouble() <= 2.5) ||
                                 ui->tableWidget->item(row, 4)->text() == selectedAspectRatio);
        bool matchQuality = (selectedQuality == "All" || ui->tableWidget->item(row, 5)->text() == selectedQuality);

        ui->tableWidget->setRowHidden(row, !(matchDecade && matchAspectRatio && matchQuality));
    }
}

void MainWindow::openImdbPage(const QString &title, const QString &year)
{
    QString name = title + " " + year;
    QString url = "https://www.imdb.com/find/?q=" + QUrl::toPercentEncoding(name);
    QDesktopServices::openUrl(QUrl(url));
}

void MainWindow::openPahePage(const QString &title, const QString &year)
{
    QString name = title + " " + year;
    QString url = "https://pahe.ink/?s=" + QUrl::toPercentEncoding(name);
    QDesktopServices::openUrl(QUrl(url));
}

void MainWindow::addComboBoxItemIfNotExist(QComboBox *comboBox, const QString &item)
{
    if (comboBox->findText(item) == -1) comboBox->addItem(item);
}

void MainWindow::addComboBoxItemsSorted(QComboBox *comboBox, const QSet<QString> &items, const QString &additionalItem)
{
    QList<double> numericRatios;
    QStringList otherItems;

    for (const QString &item : items)
    {
        if (item == "Unknown") continue;

        bool ok;
        double value = item.toDouble(&ok);
        if (ok)
        {
            if (value >= 2.2 && value <= 2.5) continue; // Skip UltraWide
            numericRatios.append(value);
        }
        else
        {
            otherItems.append(item);
        }
    }

    std::sort(numericRatios.begin(), numericRatios.end());
    std::sort(otherItems.begin(), otherItems.end());

    comboBox->clear();
    addComboBoxItemIfNotExist(comboBox, "All");

    for (double val : numericRatios)
        comboBox->addItem(QString::number(val, 'f', 2));

    comboBox->addItems(otherItems);

    if (!additionalItem.isEmpty() && comboBox->findText(additionalItem) == -1)
        comboBox->addItem(additionalItem);
}

void MainWindow::exportToExcel()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Save as Excel", "", "CSV Files (*.csv)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Could not open file for writing");
        return;
    }

    QTextStream out(&file);

    // Write headers
    QStringList headers;
    for (int col = 0; col < ui->tableWidget->columnCount(); ++col)
    {
        if (col != 10) // Skip Actions column
            headers << ui->tableWidget->horizontalHeaderItem(col)->text();
    }
    out << headers.join(",") << "\n";

    // Write rows
    for (int row = 0; row < ui->tableWidget->rowCount(); ++row)
    {
        if (ui->tableWidget->isRowHidden(row)) continue;

        QStringList rowContents;
        for (int col = 0; col < ui->tableWidget->columnCount(); ++col)
        {
            if (col == 10) continue; // Skip Actions column

            QTableWidgetItem *item = ui->tableWidget->item(row, col);
            rowContents << (item ? "\"" + item->text().replace("\"", "\"\"") + "\"" : "");
        }
        out << rowContents.join(",") << "\n";
    }

    file.close();
    QMessageBox::information(this, "Export", "Export completed successfully.");
}
