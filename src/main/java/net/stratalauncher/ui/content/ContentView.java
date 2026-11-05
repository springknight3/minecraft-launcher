package net.stratalauncher.ui.content;

import javafx.application.Platform;
import javafx.beans.binding.Bindings;
import javafx.collections.FXCollections;
import javafx.geometry.Insets;
import javafx.geometry.Pos;
import javafx.scene.control.Button;
import javafx.scene.control.ComboBox;
import javafx.scene.control.Label;
import javafx.scene.control.ListCell;
import javafx.scene.control.ListView;
import javafx.scene.control.ProgressBar;
import javafx.scene.control.TextField;
import javafx.scene.control.SelectionMode;
import javafx.scene.layout.BorderPane;
import javafx.scene.layout.HBox;
import javafx.scene.layout.Priority;
import javafx.scene.layout.VBox;
import net.stratalauncher.core.instance.Instance;
import net.stratalauncher.core.instance.InstanceManager;
import net.stratalauncher.core.loader.LoaderInstaller;
import net.stratalauncher.core.loader.LoaderSelection;
import net.stratalauncher.core.loader.LoaderType;
import net.stratalauncher.core.modrinth.ContentManager;
import net.stratalauncher.core.modrinth.ContentType;
import net.stratalauncher.core.modrinth.ModrinthClient;
import net.stratalauncher.ui.IconAssets;
import net.stratalauncher.core.auth.LauncherSettings;

import java.io.IOException;
import java.util.List;
import java.util.concurrent.atomic.AtomicLong;

public final class ContentView extends BorderPane {
    private static final int PAGE_SIZE = 20;

    private final InstanceManager instanceManager;
    private final LoaderInstaller loaderInstaller;
    private final ModrinthClient modrinthClient;
    private final ContentManager contentManager;
    private final LauncherSettings launcherSettings;
    private final ComboBox<Instance> instanceBox = new ComboBox<>();
    private final ComboBox<ContentType> typeBox = new ComboBox<>(FXCollections.observableArrayList(ContentType.values()));
    private final ComboBox<String> worldBox = new ComboBox<>();
    private final TextField searchField = new TextField();
    private final ListView<ModrinthClient.Project> projectsList = new ListView<>();
    private final ComboBox<ModrinthClient.ProjectVersion> versionBox = new ComboBox<>();
    private final ListView<ContentManager.InstalledContent> installedList = new ListView<>();
    private final Label statusLabel = new Label();
    private final Button previousButton = new Button("‹");
    private final Button nextButton = new Button("›");
    private final HBox pageButtons = new HBox(8);
    private final ProgressBar progressBar = new ProgressBar(0);
    private final AtomicLong searchSequence = new AtomicLong();
    private final AtomicLong versionSequence = new AtomicLong();
    private int searchOffset;
    private int totalProjects;
    private int currentPage = 1;
    private boolean updatingTypeChoices;

    public ContentView(InstanceManager instanceManager, LoaderInstaller loaderInstaller,
                      ModrinthClient modrinthClient, ContentManager contentManager,
                      LauncherSettings launcherSettings) {
        this.instanceManager = instanceManager;
        this.loaderInstaller = loaderInstaller;
        this.modrinthClient = modrinthClient;
        this.contentManager = contentManager;
        this.launcherSettings = launcherSettings;
        getStyleClass().add("instances-view");
        setPadding(new Insets(10));
        setMaxHeight(Double.MAX_VALUE);

        Label title = new Label("Content");
        title.getStyleClass().add("hero-title");
        Label description = new Label("Browse Modrinth and install compatible mods, resource packs, data packs, and shader packs into an isolated instance.");
        description.setWrapText(true);
        description.getStyleClass().add("hero-subtitle");

        instanceBox.setPromptText("Select instance");
        instanceBox.setMaxWidth(Double.MAX_VALUE);
        instanceBox.setCellFactory(ignored -> new ListCell<>() {
            @Override
            protected void updateItem(Instance item, boolean empty) {
                super.updateItem(item, empty);
                setText(empty || item == null ? null : item.name() + " · Minecraft " + item.versionId());
            }
        });
        instanceBox.setButtonCell(new ListCell<>() {
            @Override
            protected void updateItem(Instance item, boolean empty) {
                super.updateItem(item, empty);
                setText(empty || item == null ? null : item.name() + " · Minecraft " + item.versionId());
            }
        });
        typeBox.getSelectionModel().select(ContentType.MOD);

        HBox targetRow = new HBox(10, new Label("Instance"), instanceBox, new Label("Content type"), typeBox);
        targetRow.setAlignment(Pos.CENTER_LEFT);
        HBox.setHgrow(instanceBox, Priority.ALWAYS);

        searchField.setPromptText("Search Modrinth projects");
        searchField.setOnAction(event -> search(true));
        Button searchButton = new Button("Search");
        IconAssets.apply(searchButton, "refresh", launcherSettings.darkMode());
        searchButton.getStyleClass().add("primary-button");
        searchButton.setOnAction(event -> search(true));
        previousButton.getStyleClass().add("action-button");
        previousButton.setAccessibleText("Previous page");
        previousButton.setDisable(true);
        previousButton.setOnAction(event -> {
            if (searchOffset > 0) {
                searchOffset = Math.max(0, searchOffset - PAGE_SIZE);
                currentPage = searchOffset / PAGE_SIZE + 1;
                search(false);
            }
        });
        nextButton.getStyleClass().add("action-button");
        nextButton.setAccessibleText("Next page");
        nextButton.setDisable(true);
        nextButton.setOnAction(event -> {
            if (searchOffset + PAGE_SIZE < totalProjects) {
                searchOffset += PAGE_SIZE;
                currentPage = searchOffset / PAGE_SIZE + 1;
                search(false);
            }
        });
        pageButtons.setAlignment(Pos.CENTER_RIGHT);
        HBox searchRow = new HBox(10, searchField, searchButton, previousButton, pageButtons, nextButton);
        searchRow.setAlignment(Pos.CENTER_LEFT);
        HBox.setHgrow(searchField, Priority.ALWAYS);

        projectsList.setPlaceholder(new Label("Search for Modrinth projects."));
        projectsList.setFixedCellSize(-1);
        projectsList.setCellFactory(ignored -> createProjectCell());
        projectsList.getSelectionModel().selectedItemProperty().addListener((observable, oldValue, selected) ->
                loadVersions(selected));
        VBox.setVgrow(projectsList, Priority.ALWAYS);

        versionBox.setPromptText("Choose a compatible version");
        versionBox.setMaxWidth(Double.MAX_VALUE);
        versionBox.setCellFactory(ignored -> versionCell());
        versionBox.setButtonCell(versionCell());
        Button installButton = new Button("Install");
        IconAssets.apply(installButton, "download", launcherSettings.darkMode());
        installButton.getStyleClass().add("primary-button");
        installButton.setOnAction(event -> installSelected());
        worldBox.setPromptText("Select a world for data packs");
        worldBox.setMaxWidth(Double.MAX_VALUE);
        HBox selectionRow = new HBox(10, versionBox, installButton);
        selectionRow.setAlignment(Pos.CENTER_LEFT);
        HBox.setHgrow(versionBox, Priority.ALWAYS);

        installedList.setPlaceholder(new Label("No Modrinth content installed in this category."));
        installedList.getSelectionModel().setSelectionMode(SelectionMode.MULTIPLE);
        installedList.setAccessibleText("Installed content. Hold Shift while selecting to choose several items.");
        installedList.setCellFactory(ignored -> new ListCell<>() {
            @Override
            protected void updateItem(ContentManager.InstalledContent item, boolean empty) {
                super.updateItem(item, empty);
                setText(empty || item == null ? null : item.title() + " · " + item.versionNumber()
                        + (item.worldName().isBlank() ? "" : " · " + item.worldName())
                        + "\n" + item.filename());
            }
        });
        VBox.setVgrow(installedList, Priority.ALWAYS);
        Button removeButton = new Button("Remove selected");
        removeButton.getStyleClass().add("action-button");
        IconAssets.apply(removeButton, "delete", launcherSettings.darkMode());
        removeButton.setOnAction(event -> removeSelected());
        Label installedTitle = new Label("Installed content");
        installedTitle.getStyleClass().add("hero-subtitle");

        VBox browsePanel = new VBox(10, new Label("Modrinth projects"), searchRow,
                projectsList, versionBox, selectionRow);
        browsePanel.setPadding(new Insets(12));
        browsePanel.getStyleClass().add("hero-panel");
        VBox installedPanel = new VBox(10, installedTitle, installedList, removeButton);
        installedPanel.setPadding(new Insets(12));
        installedPanel.getStyleClass().add("hero-panel");
        HBox panels = new HBox(14, browsePanel, installedPanel);
        HBox.setHgrow(browsePanel, Priority.ALWAYS);
        HBox.setHgrow(installedPanel, Priority.ALWAYS);
        browsePanel.setPrefWidth(620);
        installedPanel.setPrefWidth(360);

        progressBar.setMaxWidth(Double.MAX_VALUE);
        progressBar.setVisible(false);
        statusLabel.setWrapText(true);
        statusLabel.getStyleClass().add("hero-subtitle");
        VBox content = new VBox(14, title, description, targetRow, worldBox, panels, progressBar, statusLabel);
        content.setMaxHeight(Double.MAX_VALUE);
        VBox.setVgrow(panels, Priority.ALWAYS);
        setCenter(content);

        statusLabel.textProperty().addListener((observable, oldValue, value) -> {
            boolean showing = value != null && !value.isBlank();
            statusLabel.setManaged(showing);
            statusLabel.setVisible(showing);
        });
        statusLabel.setManaged(false);
        statusLabel.setVisible(false);
        instanceBox.valueProperty().addListener((observable, oldValue, newValue) -> onTargetChanged());
        typeBox.valueProperty().addListener((observable, oldValue, newValue) -> {
            if (!updatingTypeChoices) {
                onTargetChanged();
            }
        });
    }

    public void initialize(List<Instance> instances) {
        instanceBox.setItems(FXCollections.observableArrayList(instances));
        if (!instances.isEmpty()) {
            instanceBox.setValue(instances.getFirst());
        }
    }

    public void refreshIcons() {
        IconAssets.refresh(this, launcherSettings.darkMode());
    }

    public void refresh() {
        Thread.ofVirtual().start(() -> {
            try {
                List<Instance> instances = instanceManager.listInstances();
                Platform.runLater(() -> {
                    Instance selected = instanceBox.getValue();
                    instanceBox.setItems(FXCollections.observableArrayList(instances));
                    Instance replacement = selected == null ? null : instances.stream()
                            .filter(instance -> instance.id().equals(selected.id())).findFirst().orElse(null);
                    if (replacement == null && !instances.isEmpty()) {
                        replacement = instances.getFirst();
                    }
                    instanceBox.setValue(replacement);
                });
            } catch (IOException exception) {
                showStatus("Could not load instances: " + exception.getMessage());
            }
        });
    }

    private void onTargetChanged() {
        searchSequence.incrementAndGet();
        versionSequence.incrementAndGet();
        totalProjects = 0;
        searchOffset = 0;
        currentPage = 1;
        updatePagination();
        projectsList.getItems().clear();
        versionBox.getItems().clear();
        refreshWorldsAndInstalled();
        Instance instance = instanceBox.getValue();
        if (instance != null) {
            loadCompatibleContentTypes(instance);
        }
    }

    private void loadCompatibleContentTypes(Instance instance) {
        Thread.ofVirtual().start(() -> {
            try {
                LoaderType loader = loaderInstaller.installedSelection(instance).type();
                Platform.runLater(() -> {
                    if (instanceBox.getValue() == null || !instanceBox.getValue().id().equals(instance.id())) {
                        return;
                    }
                    ContentType selectedType = typeBox.getValue();
                    List<ContentType> compatibleTypes = java.util.Arrays.stream(ContentType.values())
                            .filter(type -> type != ContentType.MOD || loader != LoaderType.VANILLA).toList();
                    updatingTypeChoices = true;
                    typeBox.setItems(FXCollections.observableArrayList(compatibleTypes));
                    typeBox.setValue(selectedType != null && compatibleTypes.contains(selectedType)
                            ? selectedType : compatibleTypes.getFirst());
                    updatingTypeChoices = false;
                    search(true);
                });
            } catch (IOException exception) {
                Platform.runLater(() -> showStatus(
                        "Could not determine compatible content types: " + exception.getMessage()));
            }
        });
    }

    private ListCell<ModrinthClient.Project> createProjectCell() {
        Label title = new Label();
        title.getStyleClass().add("project-title");
        Label description = new Label();
        description.getStyleClass().add("project-description");
        description.setWrapText(true);
        Label downloads = new Label();
        downloads.getStyleClass().add("project-downloads");
        VBox card = new VBox(8, title, description, downloads);
        card.setPadding(new Insets(8, 10, 8, 10));
        card.getStyleClass().add("project-list-item");
        card.prefWidthProperty().bind(Bindings.createDoubleBinding(
                () -> Math.max(120, projectsList.getWidth() - 28), projectsList.widthProperty()));
        description.maxWidthProperty().bind(Bindings.createDoubleBinding(
                () -> Math.max(120, projectsList.getWidth() - 60), projectsList.widthProperty()));
        return new ListCell<>() {
            @Override
            protected void updateItem(ModrinthClient.Project project, boolean empty) {
                super.updateItem(project, empty);
                if (empty || project == null) {
                    setText(null);
                    setGraphic(null);
                    return;
                }
                title.setText(project.title());
                description.setText(project.description());
                downloads.setText(String.format(java.util.Locale.ROOT, "%,d downloads", project.downloads()));
                setText(null);
                setGraphic(card);
            }
        };
    }

    private void refreshWorldsAndInstalled() {
        Instance instance = instanceBox.getValue();
        ContentType type = typeBox.getValue();
        if (instance == null || type == null) {
            worldBox.setDisable(true);
            installedList.getItems().clear();
            return;
        }
        worldBox.setVisible(type == ContentType.DATA_PACK);
        worldBox.setManaged(type == ContentType.DATA_PACK);
        Thread.ofVirtual().start(() -> {
            try {
                List<String> worlds = type == ContentType.DATA_PACK ? contentManager.worlds(instance) : List.of();
                List<ContentManager.InstalledContent> installed = contentManager.installed(instance, type);
                Platform.runLater(() -> {
                    if (instanceBox.getValue() == null || !instanceBox.getValue().id().equals(instance.id())
                            || typeBox.getValue() != type) {
                        return;
                    }
                    String selectedWorld = worldBox.getValue();
                    worldBox.setItems(FXCollections.observableArrayList(worlds));
                    if (selectedWorld != null && worlds.contains(selectedWorld)) {
                        worldBox.setValue(selectedWorld);
                    } else if (!worlds.isEmpty()) {
                        worldBox.setValue(worlds.getFirst());
                    } else {
                        worldBox.setValue(null);
                    }
                    worldBox.setDisable(worlds.isEmpty());
                    installedList.setItems(FXCollections.observableArrayList(installed));
                    if (type == ContentType.DATA_PACK && worlds.isEmpty()) {
                        statusLabel.setText("Create or launch a world before installing data packs.");
                    }
                });
            } catch (IOException exception) {
                showStatus("Could not load instance content: " + exception.getMessage());
            }
        });
    }

    private void search(boolean resetOffset) {
        Instance instance = instanceBox.getValue();
        ContentType type = typeBox.getValue();
        if (instance == null || type == null) {
            showStatus("Select an instance and content category first.");
            return;
        }
        if (resetOffset) {
            searchOffset = 0;
            currentPage = 1;
        }
        long sequence = searchSequence.incrementAndGet();
        int offset = searchOffset;
        String query = searchField.getText();
        totalProjects = 0;
        updatePagination();
        projectsList.getItems().clear();
        versionBox.getItems().clear();
        showStatus("Searching Modrinth...");
        Thread.ofVirtual().start(() -> {
            try {
                LoaderSelection loader = loaderInstaller.installedSelection(instance);
                ModrinthClient.SearchPage page = modrinthClient.search(type, query,
                        instance.versionId(), loader.type(), offset, PAGE_SIZE);
                Platform.runLater(() -> {
                    if (sequence != searchSequence.get() || instanceBox.getValue() == null
                            || !instanceBox.getValue().id().equals(instance.id()) || typeBox.getValue() != type) {
                        return;
                    }
                    projectsList.setItems(FXCollections.observableArrayList(page.projects()));
                    totalProjects = page.total();
                    currentPage = page.offset() / PAGE_SIZE + 1;
                    updatePagination();
                    showStatus("Showing " + page.projects().size() + " of " + page.total()
                            + " compatible projects, sorted by downloads.");
                });
            } catch (IOException | InterruptedException exception) {
                if (exception instanceof InterruptedException) {
                    Thread.currentThread().interrupt();
                }
                Platform.runLater(() -> {
                    if (sequence == searchSequence.get()) {
                        showStatus("Modrinth search failed: " + exception.getMessage());
                    }
                });
            }
        });
    }

    private void updatePagination() {
        int pageCount = (int) Math.ceil((double) totalProjects / PAGE_SIZE);
        pageButtons.getChildren().clear();
        if (pageCount == 0) {
            previousButton.setDisable(true);
            nextButton.setDisable(true);
            return;
        }
        previousButton.setDisable(searchOffset == 0);
        nextButton.setDisable(searchOffset + PAGE_SIZE >= totalProjects);
        int first = Math.max(1, currentPage - 2);
        int last = Math.min(pageCount, first + 4);
        first = Math.max(1, last - 4);
        for (int page = first; page <= last; page++) {
            int targetPage = page;
            Button number = new Button(Integer.toString(page));
            number.getStyleClass().add("page-number-button");
            if (page == currentPage) {
                number.getStyleClass().add("page-number-active");
                number.setDisable(true);
            }
            number.setOnAction(event -> {
                currentPage = targetPage;
                searchOffset = (targetPage - 1) * PAGE_SIZE;
                search(false);
            });
            pageButtons.getChildren().add(number);
        }
    }

    private void loadVersions(ModrinthClient.Project project) {
        long sequence = versionSequence.incrementAndGet();
        versionBox.getItems().clear();
        Instance instance = instanceBox.getValue();
        ContentType type = typeBox.getValue();
        if (project == null || instance == null || type == null) {
            return;
        }
        showStatus("Loading compatible versions for " + project.title() + "...");
        Thread.ofVirtual().start(() -> {
            try {
                LoaderSelection loader = loaderInstaller.installedSelection(instance);
                List<ModrinthClient.ProjectVersion> versions = modrinthClient.versions(project.id(),
                        instance.versionId(), loader.type(), type);
                Platform.runLater(() -> {
                    if (sequence != versionSequence.get() || projectsList.getSelectionModel().getSelectedItem() != project) {
                        return;
                    }
                    versionBox.setItems(FXCollections.observableArrayList(versions));
                    if (!versions.isEmpty()) {
                        versionBox.getSelectionModel().selectFirst();
                        showStatus(versions.size() + " compatible versions available for " + project.title() + ".");
                    } else {
                        showStatus("No compatible versions found for " + project.title() + ".");
                    }
                });
            } catch (IOException | InterruptedException exception) {
                if (exception instanceof InterruptedException) {
                    Thread.currentThread().interrupt();
                }
                Platform.runLater(() -> {
                    if (sequence == versionSequence.get()) {
                        showStatus("Could not load project versions: " + exception.getMessage());
                    }
                });
            }
        });
    }

    private void installSelected() {
        Instance instance = instanceBox.getValue();
        ContentType type = typeBox.getValue();
        ModrinthClient.Project project = projectsList.getSelectionModel().getSelectedItem();
        ModrinthClient.ProjectVersion version = versionBox.getValue();
        if (instance == null || type == null || project == null || version == null) {
            showStatus("Choose an instance, project, and compatible project version to install.");
            return;
        }
        if (type == ContentType.DATA_PACK && worldBox.getValue() == null) {
            showStatus("Create or select a world before installing a data pack.");
            return;
        }
        beginInstall(instance, type, project, version);
    }

    private void beginInstall(Instance instance, ContentType type, ModrinthClient.Project project,
                              ModrinthClient.ProjectVersion version) {
        String worldName = type == ContentType.DATA_PACK ? worldBox.getValue() : null;
        progressBar.setProgress(-1);
        progressBar.setVisible(true);
        showStatus("Installing " + project.title() + "...");
        Thread.ofVirtual().start(() -> {
            try {
                LoaderSelection loader = loaderInstaller.installedSelection(instance);
                ContentManager.InstalledContent installed = contentManager.install(instance, type, loader.type(),
                        worldName, project, version, progress -> Platform.runLater(() -> {
                            progressBar.setProgress(progress.total() == 0 ? -1 : progress.fraction());
                            statusLabel.setText(progress.currentItem());
                        }));
                Platform.runLater(() -> {
                    progressBar.setVisible(false);
                    refreshWorldsAndInstalled();
                    showStatus("Installed " + installed.title() + " " + installed.versionNumber() + ".");
                });
            } catch (IOException | InterruptedException exception) {
                if (exception instanceof InterruptedException) {
                    Thread.currentThread().interrupt();
                }
                Platform.runLater(() -> {
                    progressBar.setVisible(false);
                    showStatus("Could not install content: " + exception.getMessage());
                });
            }
        });
    }

    private void removeSelected() {
        Instance instance = instanceBox.getValue();
        List<ContentManager.InstalledContent> selectedContent =
                List.copyOf(installedList.getSelectionModel().getSelectedItems());
        if (instance == null || selectedContent.isEmpty()) {
            showStatus("Select an installed content item to remove.");
            return;
        }
        Thread.ofVirtual().start(() -> {
            List<String> removed = new java.util.ArrayList<>();
            List<String> failures = new java.util.ArrayList<>();
            for (ContentManager.InstalledContent item : selectedContent) {
                try {
                    contentManager.remove(instance, item);
                    removed.add(item.title());
                } catch (IOException exception) {
                    failures.add(item.title() + ": " + exception.getMessage());
                }
            }
            if (!removed.isEmpty()) {
                Platform.runLater(() -> {
                    refreshWorldsAndInstalled();
                    showStatus(failures.isEmpty() ? "Removed " + removed.size() + " content item(s)."
                            : "Removed " + removed.size() + " item(s); some could not be removed: "
                            + String.join("; ", failures));
                });
            } else {
                showStatus("Could not remove selected content: " + String.join("; ", failures));
            }
        });
    }

    private ListCell<ModrinthClient.ProjectVersion> versionCell() {
        return new ListCell<>() {
            @Override
            protected void updateItem(ModrinthClient.ProjectVersion item, boolean empty) {
                super.updateItem(item, empty);
                setText(empty || item == null ? null : item.versionNumber() + " · " + item.id());
            }
        };
    }

    private void showStatus(String message) {
        Platform.runLater(() -> statusLabel.setText(message));
    }
}
