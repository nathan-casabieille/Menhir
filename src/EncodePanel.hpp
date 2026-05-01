#pragma once
#include <QWidget>
#include <QComboBox>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>
#include <ASTERIXCodec/Codec.hpp>
#include "HexEditor.hpp"

class QLabel;
class QPushButton;
class QGroupBox;
class QCheckBox;

class EncodePanel : public QWidget {
    Q_OBJECT
public:
    explicit EncodePanel(const asterix::Codec& codec, QWidget* parent = nullptr);

    void setCategoryList(const std::vector<uint8_t>& cats);

signals:
    void statusMessage(const QString& msg);

private slots:
    void onCategoryChanged(int idx);
    void onVariationChanged(int idx);
    void onEncodeClicked();
    void onCopyHexClicked();
    void scheduleEncode();

private:
    const asterix::Codec& codec_;

    // Top controls
    QComboBox* catCombo_;
    QComboBox* varCombo_;

    // Item form (built dynamically)
    QWidget*      formWidget_;
    QVBoxLayout*  formLayout_;
    QScrollArea*  formScroll_;

    // Output
    HexEditor*  outputHex_;
    QLabel*     outputLabel_;
    QPushButton* copyBtn_;
    QPushButton* encodeBtn_;

    // Per-item form data
    struct FieldWidget {
        std::string field_name;
        asterix::Encoding encoding;
        int bits{0};
        double scale{1.0};
        std::string unit;
        std::map<uint64_t, std::string> table;
        QWidget* widget{nullptr};  // QLineEdit, QDoubleSpinBox, or QComboBox
    };

    struct ItemForm {
        std::string item_id;
        QCheckBox*  enableCheck{nullptr};
        QGroupBox*  box{nullptr};
        asterix::ItemType type;
        std::vector<FieldWidget> fields;
    };

    std::vector<ItemForm> itemForms_;
    uint8_t currentCat_{0};
    std::string currentVariation_;
    QTimer* encodeTimer_{nullptr};

    void buildForm(uint8_t cat, const std::string& variation);
    void clearForm();

    uint64_t fieldValue(const FieldWidget& fw) const;
    asterix::DecodedItem buildItem(const ItemForm& form, const asterix::DataItemDef& def) const;
};
