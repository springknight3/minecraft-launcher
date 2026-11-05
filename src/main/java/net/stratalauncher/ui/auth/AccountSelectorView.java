package net.stratalauncher.ui.auth;

import javafx.application.Platform;
import javafx.collections.FXCollections;
import javafx.geometry.Insets;
import javafx.geometry.Pos;
import javafx.scene.control.Button;
import javafx.scene.control.CheckBox;
import javafx.scene.control.Label;
import javafx.scene.control.ListCell;
import javafx.scene.control.ListView;
import javafx.scene.layout.BorderPane;
import javafx.scene.layout.HBox;
import javafx.scene.layout.Priority;
import javafx.scene.layout.VBox;
import net.stratalauncher.core.auth.Account;
import net.stratalauncher.core.auth.AccountManager;
import net.stratalauncher.core.auth.LauncherSettings;
import net.stratalauncher.core.auth.MicrosoftAuthService;
import net.stratalauncher.ui.FlyoutManager;
import net.stratalauncher.ui.IconAssets;
import net.stratalauncher.ui.ToggleSwitchStyle;

import java.io.IOException;

public final class AccountSelectorView extends BorderPane {
    private final AccountManager accountManager;
    private final MicrosoftAuthService microsoftAuthService;
    private final LauncherSettings settings;
    private final FlyoutManager flyouts;
    private final ListView<Account> accountList = new ListView<>();
    private final Label status = new Label();
    private final CheckBox activeToggle = new CheckBox("Set as active");
    private FlyoutManager.FlyoutHandle authFlyout;

    public AccountSelectorView(AccountManager accountManager, MicrosoftAuthService microsoftAuthService,
                               LauncherSettings settings, FlyoutManager flyouts) {
        this.accountManager = accountManager;
        this.microsoftAuthService = microsoftAuthService;
        this.settings = settings;
        this.flyouts = flyouts;
        setPadding(new Insets(10));
        setMaxHeight(Double.MAX_VALUE);
        getStyleClass().add("instances-view");

        Label title = new Label("Accounts");
        title.getStyleClass().add("hero-title");
        Button offlineButton = new Button("Add offline");
        IconAssets.apply(offlineButton, "account-add", settings.darkMode());
        offlineButton.getStyleClass().add("action-button");
        offlineButton.setOnAction(event -> addOfflineAccount());
        Button microsoftButton = new Button("Sign in with Microsoft");
        IconAssets.apply(microsoftButton, "secure", settings.darkMode());
        microsoftButton.getStyleClass().add("primary-button");
        microsoftButton.setOnAction(event -> signInWithMicrosoft());
        ToggleSwitchStyle.apply(activeToggle);
        activeToggle.setDisable(true);
        activeToggle.setOnAction(event -> {
            if (activeToggle.isSelected()) {
                selectAccount();
            } else {
                syncActiveToggle(activeToggle);
            }
        });
        Button removeButton = new Button("Remove");
        removeButton.getStyleClass().add("danger-button");
        IconAssets.apply(removeButton, "delete", settings.darkMode());
        removeButton.setOnAction(event -> removeAccount());

        HBox actions = new HBox(10, offlineButton, microsoftButton, activeToggle, removeButton);
        actions.setAlignment(Pos.CENTER_LEFT);
        accountList.setCellFactory(ignored -> new ListCell<>() {
            @Override
            protected void updateItem(Account account, boolean empty) {
                super.updateItem(account, empty);
                if (empty || account == null) {
                    setText(null);
                    return;
                }
                Account activeAccount = accountManager.activeAccount();
                String active = activeAccount != null && account.id().equals(activeAccount.id())
                        ? "  (active)" : "";
                setText(account.name() + "  ·  " + (account.type() == Account.Type.MICROSOFT
                        ? "Microsoft" : "Offline") + active);
                setGraphic(IconAssets.graphic(account.type() == Account.Type.MICROSOFT
                        ? "secure" : "secure-off", settings.darkMode()));
            }
        });
        accountList.getSelectionModel().selectedItemProperty().addListener((observable, oldValue, selected) ->
                syncActiveToggle(activeToggle));
        accountList.setPlaceholder(new Label("No accounts yet. Add an offline profile or sign in with Microsoft."));
        VBox.setVgrow(accountList, Priority.ALWAYS);
        status.setWrapText(true);
        status.getStyleClass().add("hero-subtitle");
        VBox content = new VBox(14, title, actions, accountList, status);
        content.setPadding(new Insets(10));
        content.setMaxHeight(Double.MAX_VALUE);
        VBox.setVgrow(accountList, Priority.ALWAYS);
        setCenter(content);
    }

    public void refreshAccounts() {
        String selectedId = accountList.getSelectionModel().getSelectedItem() == null
                ? null : accountList.getSelectionModel().getSelectedItem().id();
        accountList.setItems(FXCollections.observableArrayList(accountManager.accounts()));
        Account active = accountManager.activeAccount();
        Account selected = accountList.getItems().stream()
                .filter(account -> account.id().equals(selectedId))
                .findFirst().orElse(active);
        if (selected != null) {
            accountList.getSelectionModel().select(selected);
        }
        syncActiveToggle(activeToggle);
        accountList.refresh();
    }

    public void refreshIcons() {
        IconAssets.refresh(this, settings.darkMode());
        ToggleSwitchStyle.refresh(this, settings.darkMode());
        accountList.refresh();
    }

    private void addOfflineAccount() {
        flyouts.showPrompt("Add offline account",
                "Player name (1-16 letters, numbers, or underscores)", "", name -> {
            try {
                Account account = accountManager.addOffline(name);
                refreshAccounts();
                accountList.getSelectionModel().select(account);
                showStatus("Added offline account " + account.name() + ".");
            } catch (IOException | IllegalArgumentException exception) {
                flyouts.showError("Could not add offline account", exception.getMessage());
            }
        });
    }

    private void signInWithMicrosoft() {
        String clientId = settings.azureClientId();
        if (clientId.isBlank()) {
            showStatus("Set the Azure client ID in Settings before signing in with Microsoft.");
            return;
        }
        showStatus("Starting Microsoft device-code sign-in...");
        Thread.ofVirtual().start(() -> {
            try {
                MicrosoftAuthService.AuthenticatedProfile profile = microsoftAuthService.signIn(clientId,
                        deviceCode -> Platform.runLater(() -> showDeviceCode(deviceCode)));
                Account account = accountManager.addMicrosoft(profile);
                Platform.runLater(() -> {
                    if (authFlyout != null) {
                        authFlyout.close();
                        authFlyout = null;
                    }
                    refreshAccounts();
                    accountList.getSelectionModel().select(account);
                    showStatus("Signed in as " + account.name() + ".");
                });
            } catch (IOException | InterruptedException | IllegalArgumentException exception) {
                if (exception instanceof InterruptedException) {
                    Thread.currentThread().interrupt();
                }
                Platform.runLater(() -> {
                    if (authFlyout != null) {
                        authFlyout.close();
                        authFlyout = null;
                    }
                    flyouts.showError("Microsoft sign-in failed", exception.getMessage());
                    showStatus("Microsoft sign-in failed: " + exception.getMessage());
                });
            }
        });
    }

    private void showDeviceCode(MicrosoftAuthService.DeviceCode deviceCode) {
        authFlyout = flyouts.showDeviceCode("Complete Microsoft sign-in", "Open "
                + deviceCode.verificationUri()
                + " and enter this code:\n\n" + deviceCode.userCode()
                + "\n\nLeave this window open while sign-in completes.");
        showStatus("Waiting for Microsoft sign-in approval...");
    }

    private void selectAccount() {
        Account selected = accountList.getSelectionModel().getSelectedItem();
        if (selected == null) {
            showStatus("Select an account first.");
            return;
        }
        try {
            accountManager.select(selected.id());
            refreshAccounts();
            showStatus("Active account: " + selected.name() + ".");
        } catch (IOException | IllegalArgumentException exception) {
            syncActiveToggle(activeToggle);
            showStatus("Could not select account: " + exception.getMessage());
        }
    }

    private void syncActiveToggle(CheckBox toggle) {
        Account selected = accountList.getSelectionModel().getSelectedItem();
        Account active = accountManager.activeAccount();
        toggle.setDisable(selected == null);
        toggle.setSelected(selected != null && active != null && selected.id().equals(active.id()));
    }

    private void removeAccount() {
        Account selected = accountList.getSelectionModel().getSelectedItem();
        if (selected == null) {
            showStatus("Select an account to remove.");
            return;
        }
        flyouts.showConfirmation("Remove account",
                "Remove " + selected.name() + " from this launcher? This does not delete the Microsoft account.",
                confirmed -> {
            if (confirmed) {
            try {
                accountManager.remove(selected.id());
                refreshAccounts();
                showStatus("Removed " + selected.name() + ".");
            } catch (IOException | IllegalArgumentException exception) {
                flyouts.showError("Could not remove account", exception.getMessage());
            }
            }
        });
    }

    private void showStatus(String message) {
        status.setText(message);
    }
}
